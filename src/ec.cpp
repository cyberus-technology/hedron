/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
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

#include "ec.hpp"
#include "elf.hpp"
#include "hip.hpp"
#include "kp.hpp"
#include "rcu.hpp"
#include "sm.hpp"
#include "stdio.hpp"
#include "utcb.hpp"
#include "vcpu.hpp"
#include "vmx.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Ec::cache(sizeof(Ec), 32);

Ec::Ec(Pd* own, unsigned c)
    : Typed_kobject(static_cast<Space_obj*>(own)), cont(Ec::idle), pd(own), pd_user_page(own),
      cpu(static_cast<uint16>(c)), glb(true), fpu(new Kp(own))
{
    // The idle EC gets a Fpu and a KP for the Fpu, as this has the least complexity of all alternatives (e.g.
    // using an optional<Fpu> or having an Fpu that handles a nullptr in the constructor).
    trace(TRACE_SYSCALL, "EC:%p created (PD:%p Kernel)", this, own);

    regs.vmcs = nullptr;
}

Ec::Ec(Pd* own, mword sel, Pd* p, void (*f)(), unsigned c, unsigned e, mword u, mword s, int creation_flags)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, Ec::PERM_ALL, free, pre_free), cont(f), pd(p),
      pd_user_page((creation_flags & MAP_USER_PAGE_IN_OWNER) ? own : p), cpu(static_cast<uint16>(c)),
      glb(!!f), evt(e), fpu(new Kp(own))
{
    assert(u < USER_ADDR);
    assert((u & PAGE_MASK) == 0);

    // Make sure we consider the right CPUs for TLB shootdown
    pd->Space_mem::init(c);

    regs.vmcs = nullptr;

    if (not(creation_flags & CREATE_VCPU)) {
        if (glb) {
            regs.cs = SEL_USER_CODE;
            regs.ss = SEL_USER_DATA;
            regs.rfl = Cpu::EFL_IF;
            regs.rsp = s;
        } else
            regs.set_sp(s);

        utcb = make_unique<Utcb>();

        user_utcb = u;

        if (user_utcb) {
            pd_user_page->Space_mem::insert(
                u, 0, Hpt::PTE_NODELEG | Hpt::PTE_NX | Hpt::PTE_U | Hpt::PTE_W | Hpt::PTE_P,
                Buddy::ptr_to_phys(utcb.get()));
        }

        regs.dst_portal = EXC_STARTUP;

        trace(TRACE_SYSCALL, "EC:%p created (PD:%p CPU:%#x UTCB:%#lx ESP:%lx EVT:%#x)", this, p, c, u, s, e);

    } else {
        regs.dst_portal = VMI_STARTUP;
        regs.xcr0 = Cpu::XCR0_X87;
        regs.spec_ctrl = 0;

        if (Hip::feature() & Hip::FEAT_VMX) {
            mword host_cr3 = pd->hpt.root() | (Cpu::feature(Cpu::FEAT_PCID) ? pd->did : 0);

            regs.vmcs = new Vmcs(reinterpret_cast<mword>(sys_regs() + 1), pd->Space_pio::walk(), host_cr3,
                                 pd->ept, c);

            regs.nst_ctrl<Vmcs>();

            /* Host MSRs are restored in the exit path. */
            Vmcs::write(Vmcs::EXI_MSR_LD_ADDR, 0);
            Vmcs::write(Vmcs::EXI_MSR_LD_CNT, 0);

            /* allocate and register the guest MSR area */
            mword guest_msr_area_phys = Buddy::ptr_to_phys(new Msr_area);
            Vmcs::write(Vmcs::ENT_MSR_LD_ADDR, guest_msr_area_phys);
            Vmcs::write(Vmcs::ENT_MSR_LD_CNT, Msr_area::MSR_COUNT);
            Vmcs::write(Vmcs::EXI_MSR_ST_ADDR, guest_msr_area_phys);
            Vmcs::write(Vmcs::EXI_MSR_ST_CNT, Msr_area::MSR_COUNT);

            /* Allocate and configure a default sane MSR bitmap */
            msr_bitmap = make_unique<Vmx_msr_bitmap>();

            static const Msr::Register guest_accessible_msrs[] = {
                Msr::Register::IA32_FS_BASE,
                Msr::Register::IA32_GS_BASE,
                Msr::Register::IA32_KERNEL_GS_BASE,
                Msr::Register::IA32_TSC_AUX,

                // This is a read-only MSR that indicates which CPU
                // vulnerability mitigations are not required.
                //
                // This register should be configurable by userspace. See #124.
                Msr::Register::IA32_ARCH_CAP,

                // This is a read-write register that toggles
                // speculation-related features on the current hyperthread.
                //
                // This register is context-switched. See vmresume for why this
                // doesn't happen via guest_msr_area.
                Msr::Register::IA32_SPEC_CTRL,

                // This is a write-only MSR without state that can be used to
                // issue commands to the branch predictor. So far this is used
                // to trigger barriers for indirect branch prediction (see
                // IBPB).
                Msr::Register::IA32_PRED_CMD,

                // This is a write-only MSR without state that can be used to
                // invalidate CPU structures. This is (so far) only used to
                // flush the L1D cache.
                Msr::Register::IA32_FLUSH_CMD,
            };

            static const Msr::Register passthrough_guest_accessible_msrs[] = {
                // APERF and MPERF can be used by the guest to compute the average
                // effective cpu frequency between the last mwait and the next mwait.
                // See SDM 15.5.5 MPERF and APERF Under HDC.
                Msr::Register::IA32_APERF,
                Msr::Register::IA32_MPERF,
            };

            for (auto msr : guest_accessible_msrs) {
                msr_bitmap->set_exit(msr, Vmx_msr_bitmap::exit_setting::EXIT_NEVER);
            }

            if (pd->is_passthrough) {
                for (auto msr : passthrough_guest_accessible_msrs) {
                    msr_bitmap->set_exit(msr, Vmx_msr_bitmap::exit_setting::EXIT_NEVER);
                }
            }

            Vmcs::write(Vmcs::MSR_BITMAP, msr_bitmap->phys_addr());

            if (u) {
                /* Allocate and register the virtual LAPIC page and map it into user space. */
                user_vlapic = u;
                vlapic = make_unique<Vlapic>();

                mword vlapic_page_p = Buddy::ptr_to_phys(vlapic.get());

                Vmcs::write(Vmcs::APIC_VIRT_ADDR, vlapic_page_p);
                pd_user_page->Space_mem::insert(
                    u, 0, Hpt::PTE_NODELEG | Hpt::PTE_NX | Hpt::PTE_U | Hpt::PTE_W | Hpt::PTE_P,
                    vlapic_page_p);

                if (creation_flags & USE_APIC_ACCESS_PAGE) {
                    Vmcs::write(Vmcs::APIC_ACCS_ADDR, Buddy::ptr_to_phys(pd->get_access_page()));
                }
            }

            regs.vmcs->clear();
            cont = send_msg<ret_user_vmresume>;

            trace(TRACE_SYSCALL, "EC:%p created (PD:%p VMCS:%p VLAPIC:%lx)", this, p, regs.vmcs, u);
        }
    }

    assert(is_vcpu() == !!(creation_flags & CREATE_VCPU));
}

// De-constructor
Ec::~Ec()
{
    pre_free(this);

    if (is_vcpu()) {
        if (Hip::feature() & Hip::FEAT_VMX) {
            delete regs.vmcs;
        }
    } else {
        assert(not vlapic);
    }
}

void Ec::handle_hazard(mword hzd, void (*func)())
{
    if (hzd & HZD_RCU)
        Rcu::quiet();

    if (hzd & HZD_SCHED) {
        current()->cont = func;
        Sc::schedule();
    }

    if (hzd & HZD_RECALL) {
        current()->regs.clr_hazard(HZD_RECALL);

        if (func == ret_user_vmresume) {
            current()->regs.dst_portal = VMI_RECALL;
            send_msg<ret_user_vmresume>();
        }

        if (func == ret_user_sysexit)
            current()->redirect_to_iret();

        current()->regs.dst_portal = EXC_RECALL;
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_STEP) {
        current()->regs.clr_hazard(HZD_STEP);

        if (func == ret_user_sysexit)
            current()->redirect_to_iret();

        current()->regs.dst_portal = Cpu::EXC_DB;
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_DS_ES) {
        Cpu::hazard() &= ~HZD_DS_ES;
        asm volatile("mov %0, %%ds; mov %0, %%es" : : "r"(SEL_USER_DATA));
    }
}

void Ec::ret_user_sysexit()
{
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) &
                (HZD_RECALL | HZD_STEP | HZD_RCU | HZD_DS_ES | HZD_SCHED);
    if (EXPECT_FALSE(hzd))
        handle_hazard(hzd, ret_user_sysexit);

    // clang-format off
    asm volatile ("lea %[regs], %%rsp;"
                  EXPAND (LOAD_GPR)

                  // Restore the user stack and RFLAGS. SYSRET loads RFLAGS from
                  // R11. See entry_sysenter.
                  "mov %%r11, %%rsp;"
                  "mov $0x200, %%r11;"

                  "swapgs;"

                  // When sysret triggers a #GP, it is delivered before the
                  // switch to Ring3. Because we have already restored the user
                  // stack pointer, this is dangerous. We would execute Ring0
                  // code with a user accessible stack.
                  //
                  // See for example the Xen writeup about this problem:
                  // https://xenproject.org/2012/06/13/the-intel-sysret-privilege-escalation/
                  //
                  // This issue is prevented by preventing user mappings at the
                  // canonical boundary by setting USER_ADDR to one page before
                  // the boundary and thus the RIP we return to cannot be
                  // uncanonical.
                  "sysretq;" : : [regs] "m" (current()->regs) : "memory");
    // clang-format on

    UNREACHED;
}

void Ec::return_to_user()
{
    make_current();

    // Set the stack behind the iret frame in Exc_regs for entry via
    // interrupts.
    auto const kern_sp{reinterpret_cast<mword>(&exc_regs()->ss + 1)};

    // The Intel SDM Vol.3 chapter 6.14.2 describes that the Interrupt Stack
    // Frame must be 16 bytes aligned. Otherwise, the processor can arbitrarily
    // realign the RSP. Because our entry code depends on the RSP not being
    // realigned, we check for correct alignment here.
    assert(is_aligned_by_order(kern_sp, 4));
    Tss::local().sp0 = kern_sp;

    // This is where registers will be pushed in the system call entry path.
    // See entry_sysenter.
    Cpulocal::set_sys_entry_stack(sys_regs() + 1);

    // Reset the kernel stack and jump to the current continuation.
    asm volatile("mov %%gs:0, %%rsp; jmp *%[cont]" : : [cont] "q"(cont) : "memory");
    UNREACHED;
}

void Ec::ret_user_iret()
{
    // No need to check HZD_DS_ES because IRET will reload both anyway
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_STEP | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE(hzd))
        handle_hazard(hzd, ret_user_iret);

    asm volatile("lea %[regs], %%rsp\n"

                 // Load all general-purpose registers now that RSP points at
                 // the beginning of an Exc_regs structure.
                 EXPAND(LOAD_GPR)

                 // At this point, RSP points to `err` in Exc_regs. We need to
                 // skip the unused vector and error code.
                 "add %[vec_size], %%rsp\n"

                 // Now RSP points to RIP in Exc_regs. This is a normal IRET
                 // frame.
                 "swapgs\n"
                 "iretq\n"
                 :
                 : [regs] "m"(current()->regs), [vec_size] "i"(2 * PTR_SIZE)
                 : "memory");

    UNREACHED;
}

void Ec::ret_user_vmresume()
{
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE(hzd))
        handle_hazard(hzd, ret_user_vmresume);

    auto const& regs = current()->regs;

    regs.vmcs->make_current();

    if (EXPECT_FALSE(Pd::current()->stale_guest_tlb.chk(Cpu::id()))) {
        Pd::current()->stale_guest_tlb.clr(Cpu::id());

        // We have to use an INVEPT here as opposed to INVVPID, because the
        // paging structures might have changed and INVVPID does not flush
        // guest-physical mappings.
        Pd::current()->ept.invalidate();
    }

    if (EXPECT_FALSE(get_cr2() != regs.cr2)) {
        set_cr2(regs.cr2);
    }

    if (EXPECT_FALSE(not Fpu::load_xcr0(regs.xcr0))) {
        trace(TRACE_ERROR, "Refusing VM entry due to invalid XCR0: %#llx", regs.xcr0);

        // Make it look like a normal VM entry failure due to invalid guest state. The VMM receives this the
        // usual way.
        Vmcs::write(Vmcs::EXI_REASON, Vmcs::VMX_FAIL_STATE | Vmcs::VMX_ENTRY_FAILURE);

        asm volatile("jmp entry_vmx_failure");
        UNREACHED;
    }

    // If we knew for sure that SPEC_CTRL is available, we could load it via the
    // MSR area (guest_msr_area). The problem is that older CPUs may boot with a
    // microcode that doesn't expose SPEC_CTRL. It only becomes available once
    // microcode is updated. So we manually context switch it instead.
    //
    // Another complication is that userspace may set invalid bits and we don't
    // have the knowledge to sanitize the value. To avoid dying with a #GP in
    // the kernel, we just handle it and carry on.
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

Vcpu_acquire_result Ec::try_acquire_vcpu(Vcpu* vcpu)
{
    assert(Ec::current()->vcpu == nullptr);

    auto result{vcpu->try_acquire()};
    if (result.is_ok()) {
        Ec::current()->vcpu.reset(vcpu);
    }
    return result;
}

void Ec::release_vcpu()
{
    assert(Ec::current()->vcpu != nullptr);

    Ec::current()->vcpu->release(); // Release the ownership of the vCPU.
    Ec::current()->vcpu.release();  // Release the pointer to the vCPU.
}

void Ec::run_vcpu(Mtd mtd)
{
    assert(Ec::current()->vcpu != nullptr);

    Ec::current()->vcpu->mtd(mtd);
    resume_vcpu();
}

void Ec::resume_vcpu()
{
    assert(Ec::current()->vcpu != nullptr);

    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE(hzd)) {
        handle_hazard(hzd, resume_vcpu);
    }

    Ec::current()->vcpu->run();
}

void Ec::idle()
{
    for (;;) {

        mword hzd = Cpu::hazard() & (HZD_RCU | HZD_SCHED);
        if (EXPECT_FALSE(hzd))
            handle_hazard(hzd, idle);

        asm volatile("sti; hlt; cli" : : : "memory");
    }
}

void Ec::root_invoke()
{
    Eh* e = static_cast<Eh*>(Hpt::remap(Hip::root_addr, false));
    if (!Hip::root_addr || e->ei_magic != 0x464c457f || e->ei_class != ELF_CLASS || e->ei_data != 1 ||
        e->type != 2 || e->machine != ELF_MACHINE)
        die("No ELF");

    unsigned count = e->ph_count;
    current()->regs.set_pt(Cpu::id());
    current()->regs.set_ip(e->entry);
    current()->regs.set_sp(USER_ADDR - PAGE_SIZE);

    ELF_PHDR* p = static_cast<ELF_PHDR*>(Hpt::remap(Hip::root_addr + e->ph_offset, false));

    for (unsigned i = 0; i < count; i++, p++) {

        if (p->type == 1) {

            unsigned attr = ((p->flags & 0x4) ? Mdb::MEM_R : 0) | ((p->flags & 0x2) ? Mdb::MEM_W : 0) |
                            ((p->flags & 0x1) ? Mdb::MEM_X : 0);

            if (p->f_size != p->m_size || p->v_addr % PAGE_SIZE != p->f_offs % PAGE_SIZE)
                die("Bad ELF");

            mword phys = align_dn(p->f_offs + Hip::root_addr, PAGE_SIZE);
            mword virt = align_dn(p->v_addr, PAGE_SIZE);
            mword size = align_up(p->f_size, PAGE_SIZE);

            for (unsigned long o; size; size -= 1UL << o, phys += 1UL << o, virt += 1UL << o) {
                Tlb_cleanup cleanup;

                Pd::current()
                    ->delegate<Space_mem>(cleanup, &Pd::kern, phys >> PAGE_BITS, virt >> PAGE_BITS,
                                          (o = min(max_order(phys, size), max_order(virt, size))) - PAGE_BITS,
                                          attr, Space::SUBSPACE_HOST)
                    .unwrap("Failed to map roottask ELF image");

                // This code maps the initial ELF segments into the roottask. This means it is by definition
                // executed before the roottask had a chance to run. This means, we do not need to TLB flush
                // here.
                cleanup.ignore_tlb_flush();
            }
        }
    }

    // Map hypervisor information page
    {
        // Create the cleanup object in a separate scope, because ret_user_sysexit will not return. If we
        // don't do this, the destructor doesn't run.
        Tlb_cleanup cleanup;

        Pd::current()
            ->delegate<Space_mem>(cleanup, &Pd::kern, Buddy::ptr_to_phys(&PAGE_H) >> PAGE_BITS,
                                  (USER_ADDR - PAGE_SIZE) >> PAGE_BITS, 0, Mdb::MEM_R, Space::SUBSPACE_HOST)
            .unwrap("Failed to map HIP");

        // The PD is not used yet.
        cleanup.ignore_tlb_flush();
    }

    Space_obj::insert_root(Pd::current());
    Space_obj::insert_root(Ec::current());
    Space_obj::insert_root(Sc::current());

    ret_user_sysexit();
}

bool Ec::fixup(Exc_regs* regs)
{
    for (mword const* ptr = &FIXUP_S; ptr < &FIXUP_E; ptr += 2) {
        if (regs->rip != ptr[0]) {
            continue;
        }

        // Indicate that the instruction was skipped by setting the flag and
        // advance to the next instruction.
        regs->rfl |= Cpu::EFL_CF;
        regs->rip = ptr[1];

        return true;
    }

    return false;
}

void Ec::die(char const* reason, Exc_regs* r)
{
    if (not current()->is_vcpu() || current()->pd == &Pd::kern) {
        trace(0, "Killed EC:%p SC:%p V:%#lx CS:%#lx RIP:%#lx CR2:%#lx ERR:%#lx (%s)", current(),
              Sc::current(), r->vec, r->cs, r->rip, r->cr2, r->err, reason);
    } else
        trace(0, "Killed EC:%p SC:%p V:%#lx CR0:%#lx CR3:%#lx CR4:%#lx (%s)", current(), Sc::current(),
              r->vec, r->cr0_shadow, r->cr3_shadow, r->cr4_shadow, reason);

    Ec* ec = current()->rcap;

    if (ec)
        ec->cont =
            ec->cont == ret_user_sysexit ? static_cast<void (*)()>(sys_finish<Sys_regs::COM_ABT>) : dead;

    reply(dead);
}

void Ec::idl_handler()
{
    if (Ec::current()->cont == Ec::idle)
        Rcu::update();
}
