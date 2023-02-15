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
#include "cpu.hpp"
#include "dmar.hpp"
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
      kp_fpu_state(init_cfg.kp_fpu_state), cpu_id(Cpu::id()), fpu(kp_fpu_state.get())
{
    assert(Hip::feature() & Hip::FEAT_VMX);

    // Vcpu::run has to do the following things:
    // - set a proper RSP
    // - set the host CR3

    const mword io_bitmap{pd->Space_pio::walk()};
    vmcs = make_unique<Vmcs>(0, io_bitmap, 0, pd->ept, Cpu::id());

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
    regs.nst_ctrl<Vmcs>();

    vmcs->clear();
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

void Vcpu::run()
{
    // Only the owner of a vCPU is allowed to run it. This check must always come first in this function!
    assert(Atomic::load(owner) == Ec::current());

    vmcs->make_current();
    clear_exit_reason_shadow();

    if (EXPECT_FALSE(Atomic::exchange(poked, false))) {
        // Someone poked this vCPU, this means that this vCPU must return to user space as soon as possible.
        //
        // If we haven't entered the vCPU at least once since the last call to run_vcpu, we must enter it
        // now and make sure that we immediately return. We do this to update all modified vCPU state and
        // inject all pending events.
        //
        // If we already entered the vCPU at least once, we can just return to the VMM. As the exit reason we
        // use `VMX-preemption timer expired`.
        if (has_entered) {
            // Utcb::load_vmx puts different values into the intr_info and intr_error field, depending on the
            // value of regs.dst_poral. Thus we put VMI_RECALL into regs.dst_portal to make sure that we do
            // not leak the host interrupt info into the VMM.
            regs.dst_portal = VMI_RECALL;

            return_to_vmm(Vmcs::VMX_PREEMPT, Sys_regs::SUCCESS);
        }

        // We set the preemption timer to 0 to make sure that we exit immediately after entering the vCPU.
        utcb()->tsc_timeout = 0;
        mtd(Mtd(Mtd::TSC_TIMEOUT));
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
    // We always load the vCPUs FPU state in the VM entry path, thus we are not interested in the return
    // value.
    [[maybe_unused]] const bool fpu_needs_save{utcb()->save_vmx(&regs)};
    regs.mtd = 0;
    utcb()->mtd = 0;

    // Invalidate stale guest TLB entries if necessary.
    if (EXPECT_FALSE(Pd::current()->stale_guest_tlb.chk(Cpu::id()))) {
        Pd::current()->stale_guest_tlb.clr(Cpu::id());

        // We have to use an INVEPT here as opposed to INVVPID, because the paging structures might have
        // changed and INVVPID does not flush guest-physical mappings.
        Pd::current()->ept.invalidate();
    }

    // A page fault during VMX non-root mode may not update the guests CR2, thus we have to restore the guests
    // CR2 here. See Intel SDM Vol. 3 Chap. 27.1 - Architectural state before a VM exit
    if (EXPECT_FALSE(get_cr2() != utcb()->cr2)) {
        set_cr2(utcb()->cr2);
    }

    Ec::current()->save_fpu();

    // The VMCS does not contain any FPU state, thus we have to context switch it. After the VM entry the
    // guest will execute using this FPU state, which we also have to save after the VM exit.
    if (EXPECT_FALSE(not fpu.load_from_user())) {
        trace(TRACE_ERROR, "Refusing VM entry because loading the FPU state caused a #GP exception");

        set_exit_reason_shadow(Vmcs::VMX_FAIL_STATE | Vmcs::VMX_ENTRY_FAILURE);
        asm volatile("jmp entry_vmx_failure");
        UNREACHED;
    }

    // We set the guests XCR0 after loading its FPU state, because for the sake of simplicity and robustness
    // we always save and restore the whole FPU state.
    if (EXPECT_FALSE(not Fpu::load_xcr0(regs.xcr0))) {
        trace(TRACE_ERROR, "Refusing VM entry due to invalid XCR0: %#llx", utcb()->xcr0);

        set_exit_reason_shadow(Vmcs::VMX_FAIL_STATE | Vmcs::VMX_ENTRY_FAILURE);
        asm volatile("jmp entry_vmx_failure");
        UNREACHED;
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

    // Restore XCR0 before context switching the FPU, to use our own value instead of the guest's.
    Fpu::restore_xcr0();

    // The FPU content is still the state of our guest, thus to not corrupt it we immeadiately save it.
    // Currently we save the FPU too often, e.g. in case of a EXTINT we don't have to save the FPU if we
    // immediately reenter the vCPU, but we don't know whether Ec::resume_vcpu reschedules us.
    fpu.save();

    Ec::current()->load_fpu();

    uint32 exit_reason{get_exit_reason()};

    switch (exit_reason) {
    case Vmcs::VMX_EXC_NMI:
        handle_exception();
    case Vmcs::VMX_EXTINT:
        handle_extint();
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

    return_to_vmm(exit_reason, Sys_regs::SUCCESS);
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
        // The VM exit was caused by a NMI. Hedron currently can't handle this (see hedron#52), thus to make
        // this obvious we just die here.
        Ec::die("A VM exit that was caused by a NMI occured.");
    }

    return_to_vmm(Vmcs::VMX_EXC_NMI, Sys_regs::SUCCESS);
}

void Vcpu::handle_extint()
{
    const mword intr_info{Vmcs::read(Vmcs::EXI_INTR_INFO)};
    const unsigned intr_vect = intr_info & 0xff;

    if (intr_vect >= VEC_IPI) {
        Lapic::ipi_vector(intr_vect);
    } else if (intr_vect >= VEC_MSI) {
        Dmar::vector(intr_vect);
    } else if (intr_vect >= VEC_LVT) {
        Lapic::lvt_vector(intr_vect);
    } else if (intr_vect >= VEC_USER) {
        Locked_vector_info::handle_user_interrupt(intr_vect);
    }

    continue_running();
}

void Vcpu::return_to_vmm(uint32 exit_reason, Sys_regs::Status status)
{
    // We want to transfer the whole state, thus we set all MTD bits except for TLB and FPU.
    // (Utcb::load_vmx doesn't use Mtd::TLB and we already saved the FPU)
    const Mtd mtd{0x1dfffffful};

    // Utcb::load_vmx uses the Mtd bits of the given regs to determine which state to transfer, thus this time
    // we don't have to put anything into the UTCB.
    regs.mtd = mtd.val;

    // We always save the vCPUs FPU state in the VM exit path, thus we can ignore this return value.
    [[maybe_unused]] const bool fpu_needs_save{utcb()->load_vmx(&regs)};
    regs.mtd = 0;
    regs.dst_portal = 0;

    utcb()->exit_reason = exit_reason;

    has_entered = false;

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
        // executing. We send an IPI to force a VM exit.
        Lapic::send_ipi(cpu_id, VEC_IPI_RKE);
    }
}
