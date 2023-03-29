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

void Ec::handle_exc(Exc_regs* r)
{
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
    //
    // This means we can use CPU-local variables, but not exit from this handler via any path that expects
    // SWAPGS to work. Also the register state has only been saved on the current stack. This means any return
    // from this interrupt must happen via a return from this function.
    //
    // If we interrupted the kernel, we could have interrupted the kernel at any point. It could have been
    // holding an spinlock to modify a data structure. So grabbing spinlocks here is not safe.

    Cpulocal::restore_for_nmi();

    switch (r->vec) {
    case Cpu::EXC_NMI:
        panic("Received Non-Maskable Interrupt (NMI) on CPU %u at RIP %#lx", Cpu::id(), r->rip);
        break;

    case Cpu::EXC_DF:
        panic("Received Double Fault at on CPU %u at RIP %#lx", Cpu::id(), r->rip);
        break;

    default:
        panic("Unexpected interrupt received: %ld at RIP %#lx", r->vec, r->rip);
        break;
    }
}
