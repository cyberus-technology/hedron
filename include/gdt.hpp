/*
 * Global Descriptor Table (GDT)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2018 Thomas Prescher, Cyberus Technology GmbH.
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

#pragma once

#include "descriptor.hpp"
#include "selectors.hpp"

class Gdt : public Descriptor
{
private:
    uint32 val[2];

    inline void set32(Type type, Granularity gran, Size size, bool l, unsigned dpl, mword base, mword limit)
    {
        val[0] = static_cast<uint32>(base << 16 | (limit & 0xffff));
        val[1] = static_cast<uint32>((base & 0xff000000) | gran | size | (limit & 0xf0000) | l << 21 |
                                     1u << 15 | dpl << 13 | type | (base >> 16 & 0xff));
    }

    inline void set64(Type type, Granularity gran, Size size, bool l, unsigned dpl, mword base, mword limit)
    {
        set32(type, gran, size, l, dpl, base, limit);
        (this + 1)->val[0] = static_cast<uint32>(base >> 32);
        (this + 1)->val[1] = 0;
    }

    using Gdt_array = Gdt[SEL_MAX >> 3];
    static Gdt_array global_gdt;

public:
    static Gdt& gdt(uint32 sel);
    static void build();

    static constexpr size_t limit() { return sizeof(Gdt_array) - 1; }

    // Load the GDT into the GDTR.
    static inline void load()
    {
        Pseudo_descriptor desc{limit(), reinterpret_cast<mword>(&gdt(0))};
        asm volatile("lgdt %0" : : "m"(desc));
    }

    // Return the content of the GDTR.
    static inline Pseudo_descriptor store()
    {
        Pseudo_descriptor desc{0, 0};
        asm volatile("sgdt %0" : "=m"(desc));
        return desc;
    }

    // Load only the kernel part of the GDT into the GDTR.
    static inline void load_kernel_only()
    {
        Pseudo_descriptor desc{SEL_KERN - 1, reinterpret_cast<mword>(&gdt(0))};
        asm volatile("lgdt %0" : : "m"(desc));
    }

    // Returns the TSS selector for the given CPU.
    static uint16 remote_tss_selector(unsigned cpu);

    // Returns the TSS selector for the current CPU.
    static uint16 local_tss_selector();

    // Clears the busy bit in the TSS descriptor of the current CPU.
    static void unbusy_tss();
};
