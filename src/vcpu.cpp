/*
 * Virtual CPU
 *
 * Copyright (C) 2022 Sebastian Eydam, Cyberus Technology GmbH.
 *
 * This file is part of the Hedron hypervisor.
 *
 * Hedron is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hedron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#include "vcpu.hpp"

#include "atomic.hpp"
#include "counter.hpp"
#include "cpu.hpp"
#include "ec.hpp"
#include "hip.hpp"
#include "lapic.hpp"
#include "space_obj.hpp"
#include "stdio.hpp"
#include "vector_info.hpp"
#include "vectors.hpp"
#include "vmx_preemption_timer.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Vcpu::cache(sizeof(Vcpu), 32);

Vcpu::Vcpu(const Vcpu_init_config& init_cfg)
    : Typed_kobject(static_cast<Space_obj*>(init_cfg.owner_pd), init_cfg.cap_selector, Vcpu::PERM_ALL, free),
      pd(init_cfg.owner_pd), kp_vcpu_state(init_cfg.kp_vcpu_state), kp_vlapic_page(init_cfg.kp_vlapic_page),
      kp_fpu_state(init_cfg.kp_fpu_state), cpu_id(init_cfg.cpu), fpu(kp_fpu_state.get()),
      passthrough_vcpu(pd->is_passthrough)
{
    assert(Hip::feature() & Hip::FEAT_VMX);

    // Vcpu::run has to do the following things:
    // - set a proper RSP
    // - set the host CR3

    const mword io_bitmap{pd->Space_pio::walk()};
    vmcs = make_unique<Vmcs>(0, io_bitmap, 0, pd, cpu_id);

    // We restore the host MSRs in the VM Exit path, thus the VM Exit shouldn't restore any MSRs
    Vmcs::write(Vmcs::EXI_MSR_LD_ADDR, 0);
    Vmcs::write(Vmcs::EXI_MSR_LD_CNT, 0);

    // Allocate and register the guest MSR area, i.e. the area to load MSRs from during a VM Entry and to
    // store MSRs to during a VM Exit.
    guest_msr_area = make_unique<Msr_area>();
    const mword guest_msr_area_phys = Buddy::ptr_to_phys(guest_msr_area.get());
    Vmcs::write(Vmcs::ENT_MSR_LD_ADDR, guest_msr_area_phys);
    Vmcs::write(Vmcs::ENT_MSR_LD_CNT, Msr_area::MSR_COUNT);
    Vmcs::write(Vmcs::EXI_MSR_ST_ADDR, guest_msr_area_phys);
    Vmcs::write(Vmcs::EXI_MSR_ST_CNT, Msr_area::MSR_COUNT);

    // Allocate and configure a default MSR bitmap.
    msr_bitmap = make_unique<Vmx_msr_bitmap>();

    static const Msr::Register guest_accessible_msrs[] = {
        Msr::Register::IA32_FS_BASE,
        Msr::Register::IA32_GS_BASE,
        Msr::Register::IA32_KERNEL_GS_BASE,
        Msr::Register::IA32_TSC_AUX,

        // This is a read-only MSR that indicates which CPU vulnerability mitigations are not required.
        // This register should be configurable by userspace. See #124.
        Msr::Register::IA32_ARCH_CAP,

        // This is a read-write register that toggles speculation-related features on the current hyperthread.
        // This register is context-switched. See vmresume for why this doesn't happen via guest_msr_area.
        Msr::Register::IA32_SPEC_CTRL,

        // This is a write-only MSR without state that can be used to issue commands to the branch predictor.
        // So far this is used to trigger barriers for indirect branch prediction (see IBPB).
        Msr::Register::IA32_PRED_CMD,

        // This is a write-only MSR without state that can be used to invalidate CPU structures. This is (so
        // far) only used to flush the L1D cache.
        Msr::Register::IA32_FLUSH_CMD,
    };

    for (auto msr : guest_accessible_msrs) {
        msr_bitmap->set_exit(msr, Vmx_msr_bitmap::exit_setting::EXIT_NEVER);
    }

    static const Msr::Register passthrough_guest_accessible_msrs[] = {
        // APERF and MPERF can be used by the guest to compute the average effective cpu frequency between the
        // last mwait and the next mwait. See Intel SDM Vol. 3 Chap. 14.5.5 'MPERF and APERF Counters Under
        // HDC'.
        Msr::Register::IA32_APERF,
        Msr::Register::IA32_MPERF,

        Msr::Register::IA32_TSC_DEADLINE,

        Msr::Register::IA32_APIC_BASE,
    };

    // Give access to additional MSRs to the control VM.
    if (passthrough_vcpu) {
        for (auto msr : passthrough_guest_accessible_msrs) {
            msr_bitmap->set_exit(msr, Vmx_msr_bitmap::exit_setting::EXIT_NEVER);
        }
    }

    Vmcs::write(Vmcs::MSR_BITMAP, msr_bitmap->phys_addr());

    // Register the virtual LAPIC page.
    Vmcs::write(Vmcs::APIC_VIRT_ADDR, Buddy::ptr_to_phys(kp_vlapic_page->data_page()));

    // Register the APIC access page
    Vmcs::write(Vmcs::APIC_ACCS_ADDR, Buddy::ptr_to_phys(pd->get_access_page()));

    // TODO: Utcb::save_vmx takes a Cpu_regs object as parameter and does a regs->vmcs->make_current(). Thus
    // the regs must know the address of the VMCS. This is just a workaround, see hedron#252
    regs.vmcs = vmcs.get();

    // TODO: We have to keep in mind that we, if we remove the line above, also have to look into this
    // function, as it will throw an assertion if we don't set the vmcs member. See hedron#252.
    regs.nst_ctrl<Vmcs>(passthrough_vcpu);

    vmcs->clear();
}

void Vcpu::init()
{
    mword* dr = Vcpu::host_dr();

    asm volatile(
        "mov %%dr0, %[dr0]\n"
        "mov %%dr1, %[dr1]\n"
        "mov %%dr2, %[dr2]\n"
        "mov %%dr3, %[dr3]\n"
        "mov %%dr6, %[dr6]\n"
        : [dr0] "=r"(dr[0]), [dr1] "=r"(dr[1]), [dr2] "=r"(dr[2]), [dr3] "=r"(dr[3]), [dr6] "=r"(dr[4]));
}

Vcpu_acquire_result Vcpu::try_acquire()
{
    if (cpu_id != Cpu::id()) {
        return Err(Vcpu_acquire_error::bad_cpu());
    }

    if (Atomic::cmp_swap(owner, static_cast<Ec*>(nullptr), Ec::current())) {
        return Ok_void({});
    }
    return Err(Vcpu_acquire_error::busy());
}

void Vcpu::release()
{
    bool result{Atomic::cmp_swap(owner, Ec::current(), static_cast<Ec*>(nullptr))};
    assert(result);
}

void Vcpu::mtd(Mtd mtd)
{
    assert(Atomic::load(owner) == Ec::current());
    regs.mtd |= mtd.val;
}

void Vcpu::load_dr()
{
    mword const* const host_dr = Vcpu::host_dr();

    // If these assertions fail our debug register caching is broken. No function besides save_dr and load_dr
    // must touch the debug registers.
    assert_slow(get_dr0() == host_dr[0]);
    assert_slow(get_dr1() == host_dr[1]);
    assert_slow(get_dr2() == host_dr[2]);
    assert_slow(get_dr3() == host_dr[3]);
    assert_slow(get_dr6() == host_dr[4]);

    // Restore Debug Registers of the vCPU. DR7 is special and will be restored by vmlaunch/vmresume.
    //
    // We only write to the debug registers when their values have changed. This avoids the expensive mov to
    // debug register instructions in the common case where we just enter and exit the same vCPU.

    if (EXPECT_FALSE(host_dr[0] != regs.dr0)) {
        set_dr0(regs.dr0);
    }

    if (EXPECT_FALSE(host_dr[1] != regs.dr1)) {
        set_dr1(regs.dr1);
    }

    if (EXPECT_FALSE(host_dr[2] != regs.dr2)) {
        set_dr2(regs.dr2);
    }

    if (EXPECT_FALSE(host_dr[3] != regs.dr3)) {
        set_dr3(regs.dr3);
    }

    if (EXPECT_FALSE(host_dr[4] != regs.dr6)) {
        set_dr6(regs.dr6);
    }
}

void Vcpu::save_dr()
{
    mword* const host_dr = Vcpu::host_dr();

    // Save Debug Registers. DR7 is special. The CPU loads a default value on VM exit that disables all
    // debugging functionality. That means, we don't have to restore any host values here, because they are
    // not used.
    //
    // We read the debug registers only once here and cache their values, because reading them is expensive.
    host_dr[0] = regs.dr0 = get_dr0();
    host_dr[1] = regs.dr1 = get_dr1();
    host_dr[2] = regs.dr2 = get_dr2();
    host_dr[3] = regs.dr3 = get_dr3();
    host_dr[4] = regs.dr6 = get_dr6();
}

bool Vcpu::injecting_event()
{
    // The intr_info field is only valid inbound from userspace. But on the way to userspace we clear mtd and
    // won't read it.
    return (utcb()->mtd & Mtd::INJ) and (utcb()->intr_info & Vmcs::EVENT_VALID);
}

void Vcpu::synthesize_poked_exit()
{
    // Utcb::load_vmx puts different values into the intr_info and intr_error field, depending on the value of
    // regs.dst_poral. To avoid leaking the host interrupt info into the VMM, we need to tell it that we were
    // poked.
    regs.dst_portal = Vmcs::VMX_POKED;

    exit_reason_shadow = Vmcs::VMX_POKED;
}

void Vcpu::run()
{
    // Only the owner of a vCPU is allowed to run it. This check must always come first in this function!
    assert(Atomic::load(owner) == Ec::current());

    vmcs->make_current();

    // If a vCPU is in wait for SIPI state, if will not receive NMIs. Thus the CPU will block NMIs in this
    // case to signal that e.g. the TLB shootdown protocal should not wait for this CPU.
    if (utcb()->actv_state == 3 /* wait for SIPI*/) {
        Atomic::store(Cpu::block_nmis(), true);

        // Another CPU might have already sent an NMI before seeing that NMIs might not work anymore and we
        // might receive it when we already entered the geust. We promise to look at hazards before returning
        // to (host) userspace.
        Atomic::add(Counter::tlb_shootdown(), static_cast<uint16>(1));
    }

    // We check the hazards here again to avoid racyness due to our NMI handling. The following can happen:
    // VMCS_1 is the current VMCS. We receive an NMI and VMCS_1 gets trashed. We reschedule and now VMCS_2 is
    // made the current VMCS. We call vmresume without handling the NMI work.
    // To avoid this we set a hazard on the remote CPU before sending an NMI. That way either the trashed VMCS
    // or the hazard ensures that the NMI work gets done.
    Ec::handle_hazards(Ec::resume_vcpu);

    exit_reason_shadow = Optional<uint32>{};
    has_pending_mtf_trap = false;

    if (EXPECT_FALSE(Atomic::load(poked))) {
        // Someone poked this vCPU, this means that this vCPU must return to user space as soon as possible.
        //
        // If we haven't entered the vCPU at least once since the last call to run_vcpu and we have events to
        // inject, we must enter it now and make sure that we immediately return. Otherwise, the injected
        // event will be lost.
        //
        // If we already entered the vCPU at least once or there is nothing to inject (and thus nothing to
        // lose), we can just return to the VMM with the `VMX_POKED` exit reason.
        if (has_entered or not injecting_event()) {
            // When we come here, because we entered before, we can't be injecting an event anymore.
            assert_slow(not injecting_event());

            synthesize_poked_exit();
            return_to_vmm(Sys_regs::SUCCESS);
        }

        // We turn on the monitor trap flag to make the guest exit immediately after any pending event
        // injection.
        has_pending_mtf_trap = true;
    }

    has_entered = true;

    // entry_vmx in entry.S pushes the GPRs of the guest onto the stack directly after a VM exit. By making
    // the stack pointer point just behind our Sys_regs structure we make sure that the GPRs are saved in this
    // member variable.
    Vmcs::write(Vmcs::HOST_RSP, host_rsp());

    const mword host_cr3{Pd::current()->hpt.root() | (Cpu::feature(Cpu::FEAT_PCID) ? Pd::current()->did : 0)};
    Vmcs::write(Vmcs::HOST_CR3, host_cr3);

    // This a workaround until hedron#252 is resolved.
    utcb()->mtd = regs.mtd;
    utcb()->save_vmx(&regs, passthrough_vcpu);
    regs.mtd = 0;
    utcb()->mtd = 0;

    // We have to do this after loading the state above, so it's not overwritten. We also don't want to modify
    // the value in the vCPU state page so we can roll back to the value that userspace intended.
    if (EXPECT_FALSE(has_pending_mtf_trap)) {
        regs.vmx_set_cpu_ctrl0(utcb()->ctrl[0] | Vmcs::Ctrl0::CPU_MTF, passthrough_vcpu);
    }

    // Invalidate stale guest TLB entries if necessary.
    if (EXPECT_FALSE(Pd::current()->stale_guest_tlb.chk(Cpu::id()))) {
        Pd::current()->stale_guest_tlb.clr(Cpu::id());

        // We have to use an INVEPT here as opposed to INVVPID, because the paging structures might have
        // changed and INVVPID does not flush guest-physical mappings.
        Pd::current()->ept.invalidate();
    }

    // Intel VT does not context switch the CR2, thus we have to do this.
    if (EXPECT_FALSE(get_cr2() != regs.cr2)) {
        set_cr2(regs.cr2);
    }

    Ec::current()->save_fpu();

    // The VMCS does not contain any FPU state, thus we have to context switch it. After the VM entry the
    // guest will execute using this FPU state, which we also have to save after the VM exit.
    if (EXPECT_FALSE(not fpu.load_from_user())) {
        trace(TRACE_ERROR, "Refusing VM entry because loading the FPU state caused a #GP exception");

        exit_reason_shadow = Vmcs::VMX_FAIL_STATE | Vmcs::VMX_ENTRY_FAILURE;
        asm volatile("jmp entry_vmx_failure");
        UNREACHED;
    }

    // We set the guests XCR0 after loading its FPU state, because for the sake of simplicity and robustness
    // we always save and restore the whole FPU state.
    if (EXPECT_FALSE(not Fpu::load_xcr0(regs.xcr0))) {
        trace(TRACE_ERROR, "Refusing VM entry due to invalid XCR0: %#llx", regs.xcr0);

        exit_reason_shadow = Vmcs::VMX_FAIL_STATE | Vmcs::VMX_ENTRY_FAILURE;
        asm volatile("jmp entry_vmx_failure");
        UNREACHED;
    }

    load_dr();

    // If we knew for sure that SPEC_CTRL is available, we could load it via the MSR area (guest_msr_area).
    // The problem is that older CPUs may boot with a microcode that doesn't expose SPEC_CTRL. It only becomes
    // available once microcode is updated. So we manually context switch it instead.
    //
    // Another complication is that userspace may set invalid bits and we don't have the knowledge to sanitize
    // the value. To avoid dying with a #GP in the kernel, we just handle it and carry on.
    if (EXPECT_TRUE(Cpu::feature(Cpu::FEAT_IA32_SPEC_CTRL)) and regs.spec_ctrl != 0) {
        Msr::write_safe(Msr::IA32_SPEC_CTRL, regs.spec_ctrl);
    }

    // clang-format off
    asm volatile ("lea %[regs], %%rsp;"
                  EXPAND (LOAD_GPR)
                  "vmresume;"
                  "vmlaunch;"

                  // If we return from vmlaunch, we have an VM entry
                  // failure. Deflect this to the normal exit handling.

                  "mov %[exi_reason], %%ecx;"
                  "mov %[fail_vmentry], %%eax;"
                  "vmwrite %%rax, %%rcx;"

                  "jmp entry_vmx_failure;"

                  :
                  : [regs] "m" (regs),
                    [exi_reason] "i" (Vmcs::EXI_REASON),
                    [fail_vmentry] "i" (Vmcs::VMX_FAIL_VMENTRY)
                  : "memory");
    // clang-format on

    UNREACHED;
}

void Vcpu::handle_vmx()
{
    // As a precaution we check whether it is really the vCPUs owner that is currently executing.
    assert(Atomic::load(owner) == Ec::current());

    // Unblock NMIs if we blocked them due to entering the vCPU in wait for SIPI state.
    Atomic::store(Cpu::block_nmis(), false);

    // To defend against Spectre v2 other kernels would stuff the return stack buffer (RSB) here to avoid the
    // guest injecting branch targets. This is not necessary for us, because we start from a fresh stack and
    // do not execute RET instructions without having a matching CALL.

    // See the corresponding check in Vcpu::run for the rationale of manually context switching
    // IA32_SPEC_CTRL.
    if (EXPECT_TRUE(Cpu::feature(Cpu::FEAT_IA32_SPEC_CTRL))) {
        mword const guest_spec_ctrl = Msr::read(Msr::IA32_SPEC_CTRL);

        regs.spec_ctrl = guest_spec_ctrl;

        // Don't leak the guests SPEC_CTRL settings into the host and disable
        // all hardware-based mitigations.  We do this early to avoid
        // performance penalties due to enabled mitigation features.
        if (guest_spec_ctrl != 0) {
            Msr::write(Msr::IA32_SPEC_CTRL, 0);
        }
    }

    // The VM exit forces the GDT limit to 0xFFFF. We need to make sure this matches our GDT.
    static_assert(Gdt::limit() == 0xffff);

    // Restore XCR0 before context switching the FPU, to use our own value instead of the guest's.
    Fpu::restore_xcr0();

    // The FPU content is still the state of our guest, thus to not corrupt it we immeadiately save it.
    // Currently we save the FPU too often, e.g. in case of a EXTINT we don't have to save the FPU if we
    // immediately reenter the vCPU, but we don't know whether Ec::resume_vcpu reschedules us.
    fpu.save();

    Ec::current()->load_fpu();

    save_dr();

    uint16 basic_exit_reason{static_cast<uint16>(exit_reason() & 0xffff)};

    if (EXPECT_FALSE(has_pending_mtf_trap)
        // If userspace had MTF enabled, we should not hide the exit from it.
        and ((utcb()->ctrl[0] & Vmcs::Ctrl0::CPU_MTF) == 0)) {
        regs.vmx_set_cpu_ctrl0(utcb()->ctrl[0], passthrough_vcpu);

        // Even when we enable MTF, we might get different exits due to event injection failures. We only want
        // to hide our MTF exit, because it is an implementation detail of how poke currently works.
        if (basic_exit_reason == Vmcs::VMX_MTF) {
            synthesize_poked_exit();
        }
    }
    has_pending_mtf_trap = false;

    // We only care for the basic exit reason here, i.e. the first 16 bits of the exit reason.
    switch (basic_exit_reason) {
    case Vmcs::VMX_FAIL_STATE:
    case Vmcs::VMX_FAIL_VMENTRY: // We end up here, because the host state checks fail.
        maybe_handle_invalid_guest_state();
        break;
    case Vmcs::VMX_EXC_NMI:
        handle_exception();
    case Vmcs::VMX_INIT:
        // After sending the INIT-IPI, the guest will send the SIPI-IPI after 10ms. Thus to not loose any
        // SIPIs, we handle the INIT exit here.
        utcb()->actv_state = 3; // wait for SIPI state.
        regs.mtd |= Mtd::STA;
        continue_running();
    case Vmcs::VMX_PREEMPT:
        // Whenever a preemption timer exit occurs we set the value to the
        // maximum possible. This allows to always keep the preemption
        // timer active while keeping spurious timer exits for the user to
        // a minimum in case the user does not program the timer. Not
        // re-setting the timer leads to continuous timeout exits because
        // the timer value stays at 0. Keeping the timer active all the
        // time has the advantage of minimizing high-latency VMCS updates.
        vmx_timer::set(~0ull);
        break;
    }

    return_to_vmm(Sys_regs::SUCCESS);
}

void Vcpu::handle_exception()
{
    const mword vect_info{Vmcs::read(Vmcs::IDT_VECT_INFO)};

    const bool valid{(vect_info & (1u << 31)) != 0};
    if (valid) {
        // The VM exit occured during event delivery in VMX non-root operation. (See Intel SDM Vol. 3
        // Chap. 24.9.3) In this case the event was not delivered to the guest and we have to make sure that
        // the guest receives the event on the next VM entry.

        // Write the interrupt information (bits 0-11) and the valid bit (bit 31) into the VM-Entry
        // Interrupt-Information field (24.8.3)
        Vmcs::write(Vmcs::ENT_INTR_INFO, vect_info & 0x80000fffu);

        const bool deliver_error_code{(vect_info & (1u << 11)) != 0};
        if (deliver_error_code) {
            // The VM exit occured during delivery of a hardware exception that would have delivered an error
            // code on the stack, thus we have to set the VM-Entr exception error code.
            Vmcs::write(Vmcs::ENT_INTR_ERROR, Vmcs::read(Vmcs::IDT_VECT_ERROR));
        }

        const mword intr_type{(vect_info >> 8) & 0x7};
        if (intr_type >= 4 && intr_type <= 6) {
            // The interruption type is "Software interrupt", "Privileged software exception" or "Software
            // exception", thus we have to transfer the instruction length.
            Vmcs::write(Vmcs::ENT_INST_LEN, Vmcs::EXI_INST_LEN);
        }
    }

    const mword intr_info{Vmcs::read(Vmcs::EXI_INTR_INFO)};
    const unsigned intr_vect = intr_info & 0xff;
    const unsigned intr_type = (intr_info >> 8) & 0x7;

    if (intr_vect == 2u and intr_type == 2u) {
        // We received an NMI while being in VMX non-root mode. We can safely do all NMI work here. Check
        // Ec::handle_exc_altstack to learn more about our NMI handling.
        Ec::do_early_nmi_work();
        continue_running();
    }

    return_to_vmm(Sys_regs::SUCCESS);
}

void Vcpu::maybe_handle_invalid_guest_state()
{
    if (Vmcs::read(Vmcs::HOST_SEL_CS) != 0) {
        return;
    }

    // The invalid guest state was provoked by the NMI handler.
    Ec::fixup_nmi_user_trap();

    continue_running();
}

void Vcpu::return_to_vmm(Sys_regs::Status status)
{
    // We only want to write out the vCPU state to the state page when we actually entered the
    // guest. Otherwise, the state in the VMCS is stale and we would clobber the state page.
    if (has_entered) {
        // We want to transfer the whole state, except
        // - the EOI_EXIT_BITMAP and the TPR_THRESHOLD, because the hardware does not modify it
        // - Mtd::TLB, because Utcb::load_vmx does not use it
        // - Mtd::FPU, because we already saved the FPU
        Mtd mtd{~0UL & ~(Mtd::EOI | Mtd::TPR | Mtd::TLB | Mtd::FPU)};

        // We only transfer the Guest interrupt status (GUEST_INTR_STS) if the "virtual-interrupt delivery"
        // field of the VM-execution control is set. This also prevents reading these fields on CPUs where
        // they don't exist. The CPU handles reading non-existent fields gracefully, but it is a performance
        // issue.
        const bool vint_delivery_enabled{(utcb()->ctrl[0] & Vmcs::Ctrl0::CPU_SECONDARY) and
                                         (utcb()->ctrl[1] & Vmcs::Ctrl1::CPU_VINT_DELIVERY)};

        if (not vint_delivery_enabled) {
            mtd.val &= ~Mtd::VINTR;
        }

        // Utcb::load_vmx uses the Mtd bits of the given regs to determine which state to transfer, thus this
        // time we don't have to put anything into the UTCB.
        regs.mtd = mtd.val;

        utcb()->load_vmx(&regs);
        regs.mtd = 0;
        regs.dst_portal = 0;

        has_entered = false;
    }

    utcb()->exit_reason = exit_reason();

    // We can unconditionally clear the poked flag here, because we are just about to return to the VMM.
    Atomic::store(poked, false);

    // Return to the VMM. Ec::sys_finish releases the ownership of this vCPU by calling Vcpu::release.
    Ec::sys_finish(status);
}

void Vcpu::continue_running()
{
    // We don't have to clear the owner here and Ec::resume_vcpu will check the necessary hazards for us.
    Ec::current()->resume_vcpu();
}

void Vcpu::poke()
{
    if (Atomic::exchange(poked, true)) {
        // The vCPU has already been poked before. Whoever set Vcpu::poked initially has already sent the IPI.
        return;
    }

    if (Atomic::load(owner) == nullptr) {
        // The vCPU has no owner and thus is not running. We don't have to send an IPI.
        return;
    }

    if (Cpu::id() != cpu_id and Ec::remote(cpu_id) == Atomic::load(owner)) {
        // The owner of this vCPU is currently executing on another CPU, i.e. the vCPU is currently
        // executing. We send an NMI to force a VM exit.
        Lapic::send_nmi(cpu_id);
    }
}
