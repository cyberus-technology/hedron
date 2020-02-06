/*
 * Model-Specific Registers (MSR)
 *
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

#include "msr.hpp"

static bool is_safe_msr (Msr::Register msr)
{
    switch (msr) {
        // TODO We need to convert this to a whitelist.
    default:
        return true;
    };
}

bool Msr::user_write (Register msr, uint64 val)
{
    if (not is_safe_msr (msr)) {
        return false;
    }

    write_safe (msr, val);
    return true;
}

bool Msr::user_read (Register msr, uint64 &val)
{
    if (not is_safe_msr (msr)) {
        val = 0;
        return false;
    }

    val = read_safe<uint64> (msr);
    return true;
}

