/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "counter.hpp"
#include "ec.hpp"
#include "gdt.hpp"
#include "mca.hpp"

void Ec::load_fpu()
{
    // The idle EC never switches to user space and we do not use the FPU inside the kernel. Thus we can
    // skip loading or saving the FPU-state in case this EC is an idle EC to improve the performance.
    if (not is_idle_ec()) {
        fpu.load();
    }
}

void Ec::save_fpu()
{
    // See comment in Ec::load_fpu.
    if (not is_idle_ec()) {
        fpu.save();
    }
}

void Ec::transfer_fpu(Ec* from_ec)
{
    if (from_ec == this) {
        return;
    }

    from_ec->save_fpu();
    load_fpu();
}

bool Ec::handle_exc_gp(Exc_regs* r)
{
    if (fixup(r)) {
        return true;
    }

    if (Cpu::hazard() & HZD_TR) {
        Cpu::hazard() &= ~HZD_TR;

        // The VM exit has re-set the TR segment limit to 0x67. This breaks the
        // IO permission bitmap. Restore the correct value.
        Gdt::unbusy_tss();
        Tss::load();
        return true;
    }

    return false;
}

bool Ec::handle_exc_pf(Exc_regs* r)
{
    mword addr = r->cr2;

    if (r->err & Hpt::ERR_U)
        return false;

    // Kernel fault in OBJ space
    if (addr >= SPC_LOCAL_OBJ) {
        Space_obj::page_fault(addr, r->err);
        return true;
    }

    die("#PF (kernel)", r);
}

void Ec::do_early_nmi_work()
{
    // This function is called in the NMI handler (and maybe also somewhere else) and thus there are certain
    // things that must not be done in this function:
    //   - we must not access any locks or mutexes
    //   - we must not access any kernel data structures that are not atomically updated
    //
    // Keep in mind that NMIs may interrupt the kernel code at an arbitrary position. You can find my
    // information about the NMI handling in Ec::handle_exc_altstack.

    // Tell the shootdown code that we received the interrupt. We have to get to the actual shootdown
    // before we execute any user/guest code, but we can already acknowledge the shootdown.
    Atomic::add(Counter::tlb_shootdown(), static_cast<uint16>(1));
}

void Ec::do_deferred_nmi_work()
{
    // Here we are doing the work that we cannot unconditionally do inside the NMI handler. This function
    // should only be called by
    // - the NMI handler, if we were running in user space while receiving the NMI
    // - Ec::maybe_handle_deferred_nmi_work, which is called if we received an exception that may be caused by
    // the NMI handler
    // - Vcpu::handle_exception, if a VM exit was caused by an NMI
    //
    // All these cases have in common that we know that we are not holding any locks or that we interrupted
    // the kernel while manipulating some data structures and thus most actions are safe.
    //
    // We have to keep in mind though that we may not run on the normal kernel stack, but on the NMI stack.
    //
    // The caller of this function has to make sure that we can access CPU-local data.

    assert_slow(Cpulocal::is_initialized());
    assert_slow(Cpulocal::has_valid_stack());

    // Handle a stale TLB.
    if (Pd::current()->Space_mem::stale_host_tlb.chk(Cpu::id())) {
        Pd::current()->Space_mem::stale_host_tlb.clr(Cpu::id());
        Hpt::flush();
    }
}

void Ec::maybe_handle_deferred_nmi_work(Exc_regs* r)
{
    if (EXPECT_TRUE(r->vec != Cpu::EXC_GP)) {
        // To handle an NMI we always generate a #GP, thus if we are currently not handling a #GP, we can
        // return.
        return;
    }

    // The NMI handler has synthesized a #GP. This happens when the NMI would have returned to user space
    // directly.
    const bool synthetic_gp{r->err == NMI_SYNTH_GP_VEC};

    // The NMI handler should only synthesize a #GP if the NMI occured while the CPU is in user space.
    assert((not synthetic_gp) or r->cs == SEL_USER_CODE);
    assert(not(synthetic_gp and r->cs == SEL_KERN_CODE));

    // At this point, it is safe again to interact with the rest of the kernel, because we restored CPU-local
    // memory.

    if (not synthetic_gp) {
        return;
    }

    // We have deferred work from an earlier NMI.
    do_deferred_nmi_work();

    // Call Ec::ret_user_iret, which handles the hazards and returns to user space.
    Ec::ret_user_iret();
}

void Ec::handle_exc(Exc_regs* r)
{
    // Handle any deffered NMI work by calling maybe_handle_deferred_nmi_work. This function will not return
    // when the reason for the exception was pending work from the NMI.
    maybe_handle_deferred_nmi_work(r);

    // If we get here, CPU-local memory is initialized and kernel data structures can be accessed.
    assert_slow(Cpulocal::is_initialized());
    assert_slow(Cpulocal::has_valid_stack());
    assert_slow(r->vec == r->dst_portal);

    switch (r->vec) {

    case Cpu::EXC_GP:
        if (handle_exc_gp(r))
            return;
        break;

    case Cpu::EXC_PF:
        if (handle_exc_pf(r))
            return;
        break;

    case Cpu::EXC_MC:
        Mca::vector();
        break;
    }

    if (r->user())
        send_msg<ret_user_iret>();

    die("EXC", r);
}

void Ec::handle_exc_altstack(Exc_regs* r)
{
    // When we enter here, the GS base and KERNEL_GS_BASE MSR are not set up for kernel use. We restore GS
    // base and leave KERNEL_GS_BASE as is.

    const mword old_gs_base{rdgsbase()};

    // This means we can use CPU-local variables, but not exit from this handler via any path that expects
    // SWAPGS to work. Also the register state has only been saved on the current stack. This means any return
    // from this interrupt must happen via a return from this function.
    //
    // If we interrupted the kernel, we could have interrupted the kernel at any point. It could have been
    // holding an spinlock to modify a data structure. So grabbing spinlocks here is not safe.

    Cpulocal::restore_for_nmi();

    switch (r->vec) {
    case Cpu::EXC_NMI:

        do_early_nmi_work();
        if (r->user() /* from userspace */) {
            // Cpulocal::restore_for_nmi has changed GS_BASE, thus we have to restore the old_gs_base and then
            // call swapgs() to make GS_BASE/GS_BASE_KERNEL look like the kernel.
            wrgsbase(old_gs_base);
            swapgs();

            // We came from user space, thus the whole GDT must be loaded.
            assert_slow(Gdt::store().limit == Gdt::limit());

            // We are about to generate a synthetic #GP.

            mword kern_entry_rsp{Tss::local().sp0};
            auto* stack_ptr{reinterpret_cast<Exc_regs*>(kern_entry_rsp)};

            // This is the stack frame that the CPU would have pushed for a normal entry into the kernel.
            stack_ptr->ss = r->ss;
            stack_ptr->rsp = r->rsp;
            stack_ptr->rfl = r->rfl;
            stack_ptr->cs = r->cs;
            stack_ptr->rip = r->rip;
            stack_ptr->vec = Cpu::EXC_GP;
            stack_ptr->err = NMI_SYNTH_GP_VEC;

            stack_ptr->rax = r->rax;
            stack_ptr->rcx = r->rcx;
            stack_ptr->rdx = r->rdx;
            stack_ptr->rbx = r->rbx;
            stack_ptr->rsp = r->rsp;
            stack_ptr->rbp = r->rbp;
            stack_ptr->rsi = r->rsi;
            stack_ptr->rdi = r->rdi;
            stack_ptr->r8 = r->r8;
            stack_ptr->r9 = r->r9;
            stack_ptr->r10 = r->r10;
            stack_ptr->r11 = r->r11;
            stack_ptr->r12 = r->r12;
            stack_ptr->r13 = r->r13;
            stack_ptr->r14 = r->r14;
            stack_ptr->r15 = r->r15;

            // This is the stack frame we construct for iret.
            mword iret_frame[5];
            iret_frame[0] = reinterpret_cast<mword>(entry_from_nmi); // rip
            iret_frame[1] = SEL_KERN_CODE;                           // cs
            iret_frame[2] = 0x2;                                     // rflags
            iret_frame[3] = reinterpret_cast<mword>(stack_ptr);      // rsp
            iret_frame[4] = SEL_KERN_DATA;                           // ss

            asm volatile("lea %0, %%rsp;"
                         "iretq;"
                         :
                         : "m"(iret_frame[0])
                         : "memory");
        }
        break;

    case Cpu::EXC_DF:
        panic("Received Double Fault at on CPU %u at RIP %#lx", Cpu::id(), r->rip);
        break;

    default:
        panic("Unexpected interrupt received: %ld at RIP %#lx", r->vec, r->rip);
        break;
    }
}
