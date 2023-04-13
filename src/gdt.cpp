/*
 * Global Descriptor Table (GDT)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "gdt.hpp"
#include "cpu.hpp"
#include "cpulocal.hpp"
#include "memory.hpp"
#include "tss.hpp"

Gdt::Gdt_array Gdt::global_gdt;

Gdt& Gdt::gdt(uint32 sel) { return global_gdt[sel >> 3]; }

void Gdt::build()
{
    gdt(SEL_KERN_CODE).set32(CODE_XRA, PAGES, BIT_16, true, 0, 0, ~0ul);
    gdt(SEL_KERN_DATA).set32(DATA_RWA, PAGES, BIT_16, true, 0, 0, ~0ul);

    gdt(SEL_USER_CODE).set32(CODE_XRA, PAGES, BIT_16, true, 3, 0, ~0ul);
    gdt(SEL_USER_DATA).set32(DATA_RWA, PAGES, BIT_16, true, 3, 0, ~0ul);
    gdt(SEL_USER_CODE_L).set32(CODE_XRA, PAGES, BIT_16, true, 3, 0, ~0ul);

    for (unsigned cpu = 0; cpu < NUM_CPU; cpu++) {
        mword const tss_addr{reinterpret_cast<mword>(&Tss::remote(cpu))};

        gdt(remote_tss_selector(cpu))
            .set64(SYS_TSS, BYTES, BIT_16, false, 0, tss_addr, SPC_LOCAL_IOP_E - tss_addr);
    }
}

uint16 Gdt::remote_tss_selector(unsigned cpu) { return static_cast<uint16>(SEL_TSS_CPU0 + cpu * 0x10); }

uint16 Gdt::local_tss_selector() { return remote_tss_selector(Cpu::id()); }

void Gdt::unbusy_tss() { gdt(local_tss_selector()).val[1] &= ~0x200; }
