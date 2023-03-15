/*
 * Interrupt Descriptor Table (IDT)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "idt.hpp"
#include "extern.hpp"
#include "selectors.hpp"

ALIGNED(8) Idt Idt::idt[VEC_MAX];

void Idt::set(Idt::Type type, unsigned dpl, unsigned selector, mword offset)
{
    val[0] = static_cast<uint32>(selector << 16 | (offset & 0xffff));
    val[1] = static_cast<uint32>((offset & 0xffff0000) | 1u << 15 | dpl << 13 | type);
    val[2] = static_cast<uint32>(offset >> 32);
    val[3] = 0;
}

void Idt::build()
{
    mword* ptr = handlers;

    for (unsigned vector = 0; vector < VEC_MAX; vector++, ptr++) {
        if (*ptr) {
            idt[vector].set(SYS_INTR_GATE, *ptr & 3, SEL_KERN_CODE, *ptr & ~3);
        } else {
            idt[vector].set(SYS_TASK_GATE, 0, SEL_TSS_RUN, 0);
        }
    }
}

void Idt::load()
{
    Pseudo_descriptor desc{sizeof(idt) - 1, reinterpret_cast<mword>(idt)};
    asm volatile("lidt %0" : : "m"(desc));
}
