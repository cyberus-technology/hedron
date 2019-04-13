/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "ec.hpp"
#include "gdt.hpp"
#include "mca.hpp"
#include "stdio.hpp"

void Ec::load_fpu()
{
    fpu.load();
}

void Ec::save_fpu()
{
    fpu.save();
}

void Ec::transfer_fpu (Ec *from_ec)
{
    if (from_ec == this)
        return;

    if (!from_ec->is_idle_ec()) {
        from_ec->save_fpu();
    }

    if (!is_idle_ec()) {
        load_fpu();
    }
}

bool Ec::handle_exc_gp (Exc_regs *r)
{
    if (fixup (r->REG(ip))) {
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

bool Ec::handle_exc_pf (Exc_regs *r)
{
    mword addr = r->cr2;

    if (r->err & Hpt::ERR_U)
        return addr < USER_ADDR && Pd::current->Space_mem::loc[Cpu::id()].sync_user (Pd::current->Space_mem::hpt, addr);

    if (addr >= LINK_ADDR && addr < CPU_LOCAL && Pd::current->Space_mem::loc[Cpu::id()].sync_from (Hptp (reinterpret_cast<mword>(&PDBR)), addr, CPU_LOCAL))
        return true;

    // Kernel fault in I/O space
    if (addr >= SPC_LOCAL_IOP && addr <= SPC_LOCAL_IOP_E) {
        Space_pio::page_fault (addr, r->err);
        return true;
    }

    // Kernel fault in OBJ space
    if (addr >= SPC_LOCAL_OBJ) {
        Space_obj::page_fault (addr, r->err);
        return true;
    }

    die ("#PF (kernel)", r);
}

void Ec::handle_exc (Exc_regs *r)
{
    Counter::exc[r->vec]++;

    switch (r->vec) {

        case Cpu::EXC_GP:
            if (handle_exc_gp (r))
                return;
            break;

        case Cpu::EXC_PF:
            if (handle_exc_pf (r))
                return;
            break;

        case Cpu::EXC_MC:
            Mca::vector();
            break;
    }

    if (r->user())
        send_msg<ret_user_iret>();

    die ("EXC", r);
}
