/*
 * ACPI Sleep State Support
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "acpi.hpp"
#include "atomic.hpp"
#include "lapic.hpp"
#include "suspend.hpp"

void Suspend::suspend(uint8 slp_typa, uint8 slp_typb)
{
    if (not Acpi::valid_sleep_type (slp_typa, slp_typb)) {
        return;
    }

    if (Atomic::exchange(Suspend::in_progress, true)) {
        // Someone else is already trying to sleep.
        return;
    }

    // Nothing implemented yet.
    (void)slp_typa;
    (void)slp_typb;

    Atomic::store(Suspend::in_progress, false);
}

void Suspend::resume_bsp()
{
    // Restore the memory that we temporarily used to store our assembly resume
    // trampoline. We have to restore it early before the LAPIC code will use
    // the same memory to host its application processor boot code.
    Lapic::restore_low_memory();

    Atomic::store (Suspend::in_progress, false);
}
