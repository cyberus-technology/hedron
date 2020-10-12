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
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#include "ec.hpp"
#include "elf.hpp"
#include "hip.hpp"
#include "rcu.hpp"
#include "stdio.hpp"
#include "svm.hpp"
#include "vmx.hpp"
#include "sm.hpp"

INIT_PRIORITY (PRIO_SLAB)
Slab_cache Ec::cache (sizeof (Ec), 32);

Ec::Ec (Pd *own, unsigned c)
    : Typed_kobject (static_cast<Space_obj *>(own)), cont (Ec::idle), pd (own), pd_user_page (own), cpu (static_cast<uint16>(c)), glb (true)
{
    trace (TRACE_SYSCALL, "EC:%p created (PD:%p Kernel)", this, own);

    regs.vmcs = nullptr;
    regs.vmcb = nullptr;
}

Ec::Ec (Pd *own, mword sel, Pd *p, void (*f)(), unsigned c, unsigned e, mword u, mword s, int creation_flags)
    : Typed_kobject (static_cast<Space_obj *>(own), sel, Ec::PERM_ALL, free, pre_free), cont (f), pd (p),
      pd_user_page ((creation_flags & MAP_USER_PAGE_IN_OWNER) ? own : p),
      cpu (static_cast<uint16>(c)), glb (!!f), evt (e)
{
    assert (u < USER_ADDR);
    assert ((u & PAGE_MASK) == 0);

    // Make sure we consider the right CPUs for TLB shootdown
    pd->Space_mem::init (c);

    regs.vmcs = nullptr;
    regs.vmcb = nullptr;

    if (not (creation_flags & CREATE_VCPU)) {
        if (glb) {
            regs.cs  = SEL_USER_CODE;
            regs.ds  = SEL_USER_DATA;
            regs.es  = SEL_USER_DATA;
            regs.ss  = SEL_USER_DATA;
            regs.REG(fl) = Cpu::EFL_IF;
            regs.REG(sp) = s;
        } else
            regs.set_sp (s);

        utcb = make_unique<Utcb>();

        user_utcb = u;

        if (user_utcb) {
            pd_user_page->Space_mem::insert (u, 0, Hpt::PTE_NODELEG | Hpt::PTE_NX | Hpt::PTE_U | Hpt::PTE_W | Hpt::PTE_P,
                                             Buddy::ptr_to_phys (utcb.get()));
        }

        regs.dst_portal = NUM_EXC - 2;

        trace (TRACE_SYSCALL, "EC:%p created (PD:%p CPU:%#x UTCB:%#lx ESP:%lx EVT:%#x)", this, p, c, u, s, e);

    } else {
        regs.dst_portal = NUM_VMI - 2;
        regs.xcr0 = Cpu::XCR0_X87;
        regs.spec_ctrl = 0;

        if (Hip::feature() & Hip::FEAT_VMX) {
            mword host_cr3 = pd->hpt.root() | (Cpu::feature (Cpu::FEAT_PCID) ? pd->did : 0);

            regs.vmcs = new Vmcs (reinterpret_cast<mword>(sys_regs() + 1),
                                  pd->Space_pio::walk(),
                                  host_cr3,
                                  pd->ept,
                                  c);

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

            for (auto msr : guest_accessible_msrs) {
                msr_bitmap->set_exit(msr, Vmx_msr_bitmap::exit_setting::EXIT_NEVER);
            }

            Vmcs::write(Vmcs::MSR_BITMAP, msr_bitmap->phys_addr());

            if (u) {
                /* Allocate and register the virtual LAPIC page and map it into user space. */
                user_vlapic    = u;
                vlapic         = make_unique<Vlapic>();

                mword vlapic_page_p = Buddy::ptr_to_phys(vlapic.get());

                Vmcs::write(Vmcs::APIC_VIRT_ADDR, vlapic_page_p);
                pd_user_page->Space_mem::insert (u, 0, Hpt::PTE_NODELEG | Hpt::PTE_NX | Hpt::PTE_U | Hpt::PTE_W | Hpt::PTE_P,
                                                 vlapic_page_p);

                if (creation_flags & USE_APIC_ACCESS_PAGE) {
                    Vmcs::write(Vmcs::APIC_ACCS_ADDR, Buddy::ptr_to_phys(pd->get_access_page()));
                }
            }

            regs.vmcs->clear();
            cont = send_msg<ret_user_vmresume>;

            trace (TRACE_SYSCALL, "EC:%p created (PD:%p VMCS:%p VLAPIC:%lx)", this, p, regs.vmcs, u);
        } else if (Hip::feature() & Hip::FEAT_SVM) {

            regs.REG(ax) = Buddy::ptr_to_phys (regs.vmcb = new Vmcb (pd->Space_pio::walk(), pd->npt.root()));

            regs.nst_ctrl<Vmcb>();
            cont = send_msg<ret_user_vmrun>;
            trace (TRACE_SYSCALL, "EC:%p created (PD:%p VMCB:%p)", this, p, regs.vmcb);
        }
    }

    assert (is_vcpu() == !!(creation_flags & CREATE_VCPU));
}

//De-constructor
Ec::~Ec()
{
    pre_free(this);

    if (is_vcpu()) {
        if (Hip::feature() & Hip::FEAT_VMX) {
            delete regs.vmcs;
        } else if (Hip::feature() & Hip::FEAT_SVM) {
            delete regs.vmcb;
        }
    } else {
        assert (not vlapic);
    }

}

void Ec::handle_hazard (mword hzd, void (*func)())
{
    if (hzd & HZD_RCU)
        Rcu::quiet();

    if (hzd & HZD_SCHED) {
        current()->cont = func;
        Sc::schedule();
    }

    if (hzd & HZD_RECALL) {
        current()->regs.clr_hazard (HZD_RECALL);

        if (func == ret_user_vmresume) {
            current()->regs.dst_portal = NUM_VMI - 1;
            send_msg<ret_user_vmresume>();
        }

        if (func == ret_user_vmrun) {
            current()->regs.dst_portal = NUM_VMI - 1;
            send_msg<ret_user_vmrun>();
        }

        if (func == ret_user_sysexit)
            current()->redirect_to_iret();

        current()->regs.dst_portal = NUM_EXC - 1;
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_STEP) {
        current()->regs.clr_hazard (HZD_STEP);

        if (func == ret_user_sysexit)
            current()->redirect_to_iret();

        current()->regs.dst_portal = Cpu::EXC_DB;
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_DS_ES) {
        Cpu::hazard() &= ~HZD_DS_ES;
        asm volatile ("mov %0, %%ds; mov %0, %%es" : : "r" (SEL_USER_DATA));
    }
}

void Ec::ret_user_sysexit()
{
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_STEP | HZD_RCU | HZD_DS_ES | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_sysexit);

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR RET_USER_HYP) : : "m" (current()->regs) : "memory");

    UNREACHED;
}

void Ec::return_to_user()
{
    make_current();

    // Set the stack to just behind the register block to be able to use push
    // instructions to fill it. The assertion checks whether someone destroyed
    // our fragile structure layout.
    assert (static_cast<void *>(exc_regs() + 1) == &exc_regs()->ss + 1);

    Tss::local().sp0 = reinterpret_cast<mword>(exc_regs() + 1);
    Cpulocal::set_sys_entry_stack (sys_regs() + 1);

    // Reset the kernel stack and jump to the current continuation.
    asm volatile ("mov %%gs:0," EXPAND (PREG(sp);) "jmp *%0" : : "q" (cont) : "memory"); UNREACHED;
}

void Ec::ret_user_iret()
{
    // No need to check HZD_DS_ES because IRET will reload both anyway
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_STEP | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_iret);

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR LOAD_SEG swapgs; RET_USER_EXC) : : "m" (current()->regs) : "memory");

    UNREACHED;
}

void Ec::chk_kern_preempt()
{
    if (!Cpu::preemption())
        return;

    if (Cpu::hazard() & HZD_SCHED) {
        Cpu::preempt_disable();
        Sc::schedule();
    }
}

void Ec::ret_user_vmresume()
{
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_vmresume);

    auto const &regs = current()->regs;

    regs.vmcs->make_current();

    if (EXPECT_FALSE (Pd::current()->gtlb.chk (Cpu::id()))) {
        Pd::current()->gtlb.clr (Cpu::id());
        Pd::current()->ept.flush();
    }

    if (EXPECT_FALSE (get_cr2() != regs.cr2)) {
        set_cr2 (regs.cr2);
    }

    if (EXPECT_FALSE (not Fpu::load_xcr0 (regs.xcr0))) {
        die ("Invalid XCR0");
    }

    // If we knew for sure that SPEC_CTRL is available, we could load it via the
    // MSR area (guest_msr_area). The problem is that older CPUs may boot with a
    // microcode that doesn't expose SPEC_CTRL. It only becomes available once
    // microcode is updated. So we manually context switch it instead.
    //
    // Another complication is that userspace may set invalid bits and we don't
    // have the knowledge to sanitize the value. To avoid dying with a #GP in
    // the kernel, we just handle it and carry on.
    if (EXPECT_TRUE (Cpu::feature (Cpu::FEAT_IA32_SPEC_CTRL)) and regs.spec_ctrl != 0) {
        Msr::write_safe (Msr::IA32_SPEC_CTRL, regs.spec_ctrl);
    }

    asm volatile ("lea %[regs]," EXPAND (PREG(sp); LOAD_GPR)
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

    UNREACHED;
}

void Ec::ret_user_vmrun()
{
    mword hzd = (Cpu::hazard() | current()->regs.hazard()) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_vmrun);

    if (EXPECT_FALSE (Pd::current()->gtlb.chk (Cpu::id()))) {
        Pd::current()->gtlb.clr (Cpu::id());
        current()->regs.vmcb->tlb_control = 1;
    }

    if (EXPECT_FALSE (not Fpu::load_xcr0 (current()->regs.xcr0))) {
        die ("Invalid XCR0");
    }

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR)
                  "clgi;"
                  "sti;"
                  "vmload " EXPAND (PREG(ax);)
                  "vmrun " EXPAND (PREG(ax);)
                  "vmsave " EXPAND (PREG(ax);)
                  EXPAND (SAVE_GPR)
                  "mov %1," EXPAND (PREG(ax);)
                  "mov %%gs:0," EXPAND (PREG(sp);) // Per_cpu::self
                  "vmload " EXPAND (PREG(ax);)
                  "cli;"
                  "stgi;"
                  "jmp svm_handler;"
                  : : "m" (current()->regs), "m" (Vmcb::root) : "memory");

    UNREACHED;
}

void Ec::idle()
{
    for (;;) {

        mword hzd = Cpu::hazard() & (HZD_RCU | HZD_SCHED);
        if (EXPECT_FALSE (hzd))
            handle_hazard (hzd, idle);

        uint64 t1 = rdtsc();
        asm volatile ("sti; hlt; cli" : : : "memory");
        uint64 t2 = rdtsc();

        Counter::cycles_idle() += t2 - t1;
    }
}

void Ec::root_invoke()
{
    Eh *e = static_cast<Eh *>(Hpt::remap (Hip::root_addr, false));
    if (!Hip::root_addr || e->ei_magic != 0x464c457f || e->ei_class != ELF_CLASS || e->ei_data != 1 || e->type != 2 || e->machine != ELF_MACHINE)
        die ("No ELF");

    unsigned count = e->ph_count;
    current()->regs.set_pt (Cpu::id());
    current()->regs.set_ip (e->entry);
    current()->regs.set_sp (USER_ADDR - PAGE_SIZE);

    ELF_PHDR *p = static_cast<ELF_PHDR *>(Hpt::remap (Hip::root_addr + e->ph_offset, false));

    for (unsigned i = 0; i < count; i++, p++) {

        if (p->type == 1) {

            unsigned attr =
                ((p->flags & 0x4) ? Mdb::MEM_R : 0) |
                ((p->flags & 0x2) ? Mdb::MEM_W : 0) |
                ((p->flags & 0x1) ? Mdb::MEM_X : 0);

            if (p->f_size != p->m_size || p->v_addr % PAGE_SIZE != p->f_offs % PAGE_SIZE)
                die ("Bad ELF");

            mword phys = align_dn (p->f_offs + Hip::root_addr, PAGE_SIZE);
            mword virt = align_dn (p->v_addr, PAGE_SIZE);
            mword size = align_up (p->f_size, PAGE_SIZE);

            for (unsigned long o; size; size -= 1UL << o, phys += 1UL << o, virt += 1UL << o) {
                Pd::current()->delegate<Space_mem>(&Pd::kern, phys >> PAGE_BITS, virt >> PAGE_BITS, (o = min (max_order (phys, size), max_order (virt, size))) - PAGE_BITS, attr, Space::SUBSPACE_HOST);
            }
        }
    }

    // Map hypervisor information page
    Pd::current()->delegate<Space_mem>(&Pd::kern, Buddy::ptr_to_phys (&PAGE_H) >> PAGE_BITS, (USER_ADDR - PAGE_SIZE) >> PAGE_BITS, 0, Mdb::MEM_R, Space::SUBSPACE_HOST);

    Space_obj::insert_root (Pd::current());
    Space_obj::insert_root (Ec::current());
    Space_obj::insert_root (Sc::current());

    ret_user_sysexit();
}

bool Ec::fixup (Exc_regs *regs)
{
    for (mword const *ptr = &FIXUP_S; ptr < &FIXUP_E; ptr += 2) {
        if (regs->REG(ip) != ptr[0]) {
            continue;
        }

        // Indicate that the instruction was skipped by setting the flag and
        // advance to the next instruction.
        regs->REG(fl) |= Cpu::EFL_CF;
        regs->REG(ip) = ptr[1];

        return true;
    }

    return false;
}

void Ec::die (char const *reason, Exc_regs *r)
{
    if (not current()->is_vcpu() || current()->pd == &Pd::kern) {
        trace (0, "Killed EC:%p SC:%p V:%#lx CS:%#lx EIP:%#lx CR2:%#lx ERR:%#lx (%s)",
               current(), Sc::current(), r->vec, r->cs, r->REG(ip), r->cr2, r->err, reason);
    } else
        trace (0, "Killed EC:%p SC:%p V:%#lx CR0:%#lx CR3:%#lx CR4:%#lx (%s)",
               current(), Sc::current(), r->vec, r->cr0_shadow, r->cr3_shadow, r->cr4_shadow, reason);

    Ec *ec = current()->rcap;

    if (ec)
        ec->cont = ec->cont == ret_user_sysexit ? static_cast<void (*)()>(sys_finish<Sys_regs::COM_ABT>) : dead;

    reply (dead);
}

void Ec::idl_handler()
{
    if (Ec::current()->cont == Ec::idle)
        Rcu::update();
}
