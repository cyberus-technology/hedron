/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2013-2014 Alexander Boettcher, Genode Labs GmbH
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

#include "bits.hpp"
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

Ec *Ec::current;

// Constructors
Ec::Ec (Pd *own, void (*f)(), unsigned c) : Kobject (EC, static_cast<Space_obj *>(own)), cont (f), utcb (nullptr), pd (own), fpu (new Fpu), cpu (static_cast<uint16>(c)), glb (true), evt (0), user_utcb (0), xcpu_sm (nullptr)
{
    trace (TRACE_SYSCALL, "EC:%p created (PD:%p Kernel)", this, own);

    regs.vmcs = nullptr;
    regs.vmcb = nullptr;
}

Ec::Ec (Pd *own, mword sel, Pd *p, void (*f)(), unsigned c, unsigned e, mword u, mword s, bool vcpu, bool use_apic_access_page) : Kobject (EC, static_cast<Space_obj *>(own), sel, 0xd, free, pre_free), cont (f), pd (p), fpu (new Fpu), cpu (static_cast<uint16>(c)), glb (!!f), evt (e), user_utcb (u), xcpu_sm (nullptr)
{
    // Make sure we have a PTAB for this CPU in the PD
    pd->Space_mem::init (c);

    regs.vmcs = nullptr;
    regs.vmcb = nullptr;

    if (not vcpu) {
        if (glb) {
            regs.cs  = SEL_USER_CODE;
            regs.ds  = SEL_USER_DATA;
            regs.es  = SEL_USER_DATA;
            regs.ss  = SEL_USER_DATA;
            regs.REG(fl) = Cpu::EFL_IF;
            regs.REG(sp) = s;
        } else
            regs.set_sp (s);

        utcb = new Utcb;

        pd->Space_mem::insert (u, 0, Hpt::HPT_U | Hpt::HPT_W | Hpt::HPT_P, Buddy::ptr_to_phys (utcb));

        regs.dst_portal = NUM_EXC - 2;

        trace (TRACE_SYSCALL, "EC:%p created (PD:%p CPU:%#x UTCB:%#lx ESP:%lx EVT:%#x)", this, p, c, u, s, e);

        pd->insert_utcb (u, Buddy::ptr_to_phys(utcb) >> 12);

    } else {

        utcb = nullptr;

        regs.dst_portal = NUM_VMI - 2;

        if (Hip::feature() & Hip::FEAT_VMX) {

            regs.vmcs = new Vmcs (reinterpret_cast<mword>(sys_regs() + 1),
                                  pd->Space_pio::walk(),
                                  pd->loc[c].root(),
                                  pd->ept.root());

            regs.nst_ctrl<Vmcs>();

            /* allocate and register the host MSR area */
            mword host_msr_area_phys = Buddy::ptr_to_phys(new Msr_area);
            Vmcs::write(Vmcs::EXI_MSR_LD_ADDR, host_msr_area_phys);
            Vmcs::write(Vmcs::EXI_MSR_LD_CNT, Msr_area::MSR_COUNT);

            /* allocate and register the guest MSR area */
            mword guest_msr_area_phys = Buddy::ptr_to_phys(new Msr_area);
            Vmcs::write(Vmcs::ENT_MSR_LD_ADDR, guest_msr_area_phys);
            Vmcs::write(Vmcs::ENT_MSR_LD_CNT, Msr_area::MSR_COUNT);
            Vmcs::write(Vmcs::EXI_MSR_ST_ADDR, guest_msr_area_phys);
            Vmcs::write(Vmcs::EXI_MSR_ST_CNT, Msr_area::MSR_COUNT);

            if (u) {
                /* allocate+register the virtual LAPIC page and map it into user space */
                vlapic_page         = Buddy::allocator.alloc(0, Buddy::FILL_0);
                mword vlapic_page_p = Buddy::ptr_to_phys(vlapic_page);

                Vmcs::write(Vmcs::APIC_VIRT_ADDR, vlapic_page_p);
                pd->Space_mem::insert (u, 0, Hpt::HPT_U | Hpt::HPT_W | Hpt::HPT_P, vlapic_page_p);

                if (use_apic_access_page) {
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
}

Ec::Ec (Pd *own, Pd *p, void (*f)(), unsigned c, Ec *clone) : Kobject (EC, static_cast<Space_obj *>(own), 0, 0xd, free, pre_free), cont (f), regs (clone->regs), rcap (clone), utcb (clone->utcb), pd (p), fpu (clone->fpu), cpu (static_cast<uint16>(c)), glb (!!f), evt (clone->evt), user_utcb (0), xcpu_sm (clone->xcpu_sm)
{
    // Make sure we have a PTAB for this CPU in the PD
    pd->Space_mem::init (c);

    regs.vmcs = nullptr;
    regs.vmcb = nullptr;
}

//De-constructor
Ec::~Ec()
{
    if (xcpu_sm) {
        delete xcpu_sm;
        xcpu_sm = nullptr;
    }

    pre_free(this);

    if (fpu)
        delete fpu;

    if (utcb) {
        delete utcb;
        return;
    }

    if (vlapic_page) {
        Buddy::allocator.free(reinterpret_cast<mword>(vlapic_page));
    }

    if (Hip::feature() & Hip::FEAT_VMX)
        delete regs.vmcs;
    else if (Hip::feature() & Hip::FEAT_SVM)
        delete regs.vmcb;
}

void Ec::handle_hazard (mword hzd, void (*func)())
{
    if (hzd & HZD_RCU)
        Rcu::quiet();

    if (hzd & HZD_SCHED) {
        current->cont = func;
        Sc::schedule();
    }

    if (hzd & HZD_RECALL) {
        current->regs.clr_hazard (HZD_RECALL);

        if (func == ret_user_vmresume) {
            current->regs.dst_portal = NUM_VMI - 1;
            send_msg<ret_user_vmresume>();
        }

        if (func == ret_user_vmrun) {
            current->regs.dst_portal = NUM_VMI - 1;
            send_msg<ret_user_vmrun>();
        }

        if (func == ret_user_sysexit)
            current->redirect_to_iret();

        current->regs.dst_portal = NUM_EXC - 1;
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_STEP) {
        current->regs.clr_hazard (HZD_STEP);

        if (func == ret_user_sysexit)
            current->redirect_to_iret();

        current->regs.dst_portal = Cpu::EXC_DB;
        send_msg<ret_user_iret>();
    }

    if (hzd & HZD_DS_ES) {
        Cpu::hazard &= ~HZD_DS_ES;
        asm volatile ("mov %0, %%ds; mov %0, %%es" : : "r" (SEL_USER_DATA));
    }
}

void Ec::ret_user_sysexit()
{
    mword hzd = (Cpu::hazard | current->regs.hazard()) & (HZD_RECALL | HZD_STEP | HZD_RCU | HZD_DS_ES | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_sysexit);

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR RET_USER_HYP) : : "m" (current->regs) : "memory");

    UNREACHED;
}

void Ec::ret_user_iret()
{
    // No need to check HZD_DS_ES because IRET will reload both anyway
    mword hzd = (Cpu::hazard | current->regs.hazard()) & (HZD_RECALL | HZD_STEP | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_iret);

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR LOAD_SEG RET_USER_EXC) : : "m" (current->regs) : "memory");

    UNREACHED;
}

void Ec::chk_kern_preempt()
{
    if (!Cpu::preemption)
        return;

    if (Cpu::hazard & HZD_SCHED) {
        Cpu::preempt_disable();
        Sc::schedule();
    }
}

void Ec::ret_user_vmresume()
{
    mword hzd = (Cpu::hazard | current->regs.hazard()) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_vmresume);

    current->regs.vmcs->make_current();

    if (EXPECT_FALSE (Pd::current->gtlb.chk (Cpu::id))) {
        Pd::current->gtlb.clr (Cpu::id);
        Pd::current->ept.flush();
    }

    if (EXPECT_FALSE (get_cr2() != current->regs.cr2))
        set_cr2 (current->regs.cr2);

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR)
                  "vmresume;"
                  "vmlaunch;"
                  "mov %1," EXPAND (PREG(sp);)
                  : : "m" (current->regs), "i" (CPU_LOCAL_STCK + PAGE_SIZE) : "memory");

    trace (0, "VM entry failed with error %#lx", Vmcs::read (Vmcs::VMX_INST_ERROR));

    die ("VMENTRY");
}

void Ec::ret_user_vmrun()
{
    mword hzd = (Cpu::hazard | current->regs.hazard()) & (HZD_RECALL | HZD_TSC | HZD_RCU | HZD_SCHED);
    if (EXPECT_FALSE (hzd))
        handle_hazard (hzd, ret_user_vmrun);

    if (EXPECT_FALSE (Pd::current->gtlb.chk (Cpu::id))) {
        Pd::current->gtlb.clr (Cpu::id);
        current->regs.vmcb->tlb_control = 1;
    }

    asm volatile ("lea %0," EXPAND (PREG(sp); LOAD_GPR)
                  "clgi;"
                  "sti;"
                  "vmload;"
                  "vmrun;"
                  "vmsave;"
                  EXPAND (SAVE_GPR)
                  "mov %1," EXPAND (PREG(ax);)
                  "mov %2," EXPAND (PREG(sp);)
                  "vmload;"
                  "cli;"
                  "stgi;"
                  "jmp svm_handler;"
                  : : "m" (current->regs), "m" (Vmcb::root), "i" (CPU_LOCAL_STCK + PAGE_SIZE) : "memory");

    UNREACHED;
}

void Ec::idle()
{
    for (;;) {

        mword hzd = Cpu::hazard & (HZD_RCU | HZD_SCHED);
        if (EXPECT_FALSE (hzd))
            handle_hazard (hzd, idle);

        uint64 t1 = rdtsc();
        asm volatile ("sti; hlt; cli" : : : "memory");
        uint64 t2 = rdtsc();

        Counter::cycles_idle += t2 - t1;
    }
}

void Ec::root_invoke()
{
    Eh *e = static_cast<Eh *>(Hpt::remap (Hip::root_addr));
    if (!Hip::root_addr || e->ei_magic != 0x464c457f || e->ei_class != ELF_CLASS || e->ei_data != 1 || e->type != 2 || e->machine != ELF_MACHINE)
        die ("No ELF");

    unsigned count = e->ph_count;
    current->regs.set_pt (Cpu::id);
    current->regs.set_ip (e->entry);
    current->regs.set_sp (USER_ADDR - PAGE_SIZE);

    ELF_PHDR *p = static_cast<ELF_PHDR *>(Hpt::remap (Hip::root_addr + e->ph_offset));

    for (unsigned i = 0; i < count; i++, p++) {

        if (p->type == 1) {

            unsigned attr = !!(p->flags & 0x4) << 0 |   // R
                            !!(p->flags & 0x2) << 1 |   // W
                            !!(p->flags & 0x1) << 2;    // X

            if (p->f_size != p->m_size || p->v_addr % PAGE_SIZE != p->f_offs % PAGE_SIZE)
                die ("Bad ELF");

            mword phys = align_dn (p->f_offs + Hip::root_addr, PAGE_SIZE);
            mword virt = align_dn (p->v_addr, PAGE_SIZE);
            mword size = align_up (p->f_size, PAGE_SIZE);

            for (unsigned long o; size; size -= 1UL << o, phys += 1UL << o, virt += 1UL << o)
                Pd::current->delegate<Space_mem>(&Pd::kern, phys >> PAGE_BITS, virt >> PAGE_BITS, (o = min (max_order (phys, size), max_order (virt, size))) - PAGE_BITS, attr);
        }
    }

    // Map hypervisor information page
    Pd::current->delegate<Space_mem>(&Pd::kern, reinterpret_cast<Paddr>(&FRAME_H) >> PAGE_BITS, (USER_ADDR - PAGE_SIZE) >> PAGE_BITS, 0, 1);

    Space_obj::insert_root (Pd::current);
    Space_obj::insert_root (Ec::current);
    Space_obj::insert_root (Sc::current);

    ret_user_sysexit();
}

void Ec::handle_tss()
{
    Console::panic ("Task gate invoked");
}

bool Ec::fixup (mword &eip)
{
    for (mword *ptr = &FIXUP_S; ptr < &FIXUP_E; ptr += 2)
        if (eip == *ptr) {
            eip = *++ptr;
            return true;
        }

    return false;
}

void Ec::die (char const *reason, Exc_regs *r)
{
    if (current->utcb || current->pd == &Pd::kern) {
        if (strcmp(reason, "PT not found"))
        trace (0, "Killed EC:%p SC:%p V:%#lx CS:%#lx EIP:%#lx CR2:%#lx ERR:%#lx (%s)",
               current, Sc::current, r->vec, r->cs, r->REG(ip), r->cr2, r->err, reason);
    } else
        trace (0, "Killed EC:%p SC:%p V:%#lx CR0:%#lx CR3:%#lx CR4:%#lx (%s)",
               current, Sc::current, r->vec, r->cr0_shadow, r->cr3_shadow, r->cr4_shadow, reason);

    Ec *ec = current->rcap;

    if (ec)
        ec->cont = ec->cont == ret_user_sysexit ? static_cast<void (*)()>(sys_finish<Sys_regs::COM_ABT>) : dead;

    reply (dead);
}

void Ec::xcpu_return()
{
    assert (current->xcpu_sm);
    assert (current->rcap);
    assert (current->utcb);
    assert (Sc::current->ec == current);

    current->rcap->regs =  current->regs;

    current->xcpu_sm->up (ret_xcpu_reply);

    current->rcap    = nullptr;
    current->utcb    = nullptr;
    current->fpu     = nullptr;
    current->xcpu_sm = nullptr;

    Rcu::call(current);
    Rcu::call(Sc::current);

    Sc::schedule(true);
}

void Ec::idl_handler()
{
    if (Ec::current->cont == Ec::idle)
        Rcu::update();
}
