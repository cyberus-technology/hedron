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

uint64 Msr::read (Register msr)
{
    uint32 h, l;
    asm volatile ("rdmsr" : "=a" (l), "=d" (h) : "c" (msr));
    return (static_cast<uint64>(h) << 32) | l;
}

uint64 Msr::read_safe (Register msr)
{
    uint32 h {0}, l {0};
    asm volatile (FIXUP_CALL(rdmsr)
                  : "+a" (l), "+d" (h) : "c" (msr));
    return (static_cast<uint64>(h) << 32) | l;
}

void Msr::write (Register msr, uint64 val)
{
    asm volatile ("wrmsr" : : "a" (static_cast<mword>(val)), "d" (static_cast<mword>(val >> 32)), "c" (msr));
}

void Msr::write_safe (Register msr, uint64 val)
{
    asm volatile (FIXUP_CALL(wrmsr)
                  : : "a" (static_cast<mword>(val)), "d" (static_cast<mword>(val >> 32)), "c" (msr));
}
