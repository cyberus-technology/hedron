/*
 * Global Descriptor Table (GDT)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "cpulocal.hpp"
#include "gdt.hpp"
#include "memory.hpp"
#include "tss.hpp"

Gdt &Gdt::gdt(uint32 sel)
{
    return Cpulocal::get().gdt[sel >> 3];
}

void Gdt::build()
{
    gdt(SEL_KERN_CODE).set32 (CODE_XRA, PAGES, BIT_16, true, 0, 0, ~0ul);
    gdt(SEL_KERN_DATA).set32 (DATA_RWA, PAGES, BIT_16, true, 0, 0, ~0ul);

    gdt(SEL_USER_CODE).set32 (CODE_XRA, PAGES, BIT_16, true, 3, 0, ~0ul);
    gdt(SEL_USER_DATA).set32 (DATA_RWA, PAGES, BIT_16, true, 3, 0, ~0ul);
    gdt(SEL_USER_CODE_L).set32 (CODE_XRA, PAGES, BIT_16, true, 3, 0, ~0ul);

    gdt(SEL_TSS_RUN).set64 (SYS_TSS, BYTES, BIT_16, false, 0, reinterpret_cast<mword>(&Tss::run), SPC_LOCAL_IOP_E - reinterpret_cast<mword>(&Tss::run));
}
