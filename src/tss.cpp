/*
 * Task State Segment (TSS)
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

#include "tss.hpp"
#include "cpu.hpp"
#include "cpulocal.hpp"
#include "hpt.hpp"

static_assert((TSS_AREA_E - TSS_AREA) / sizeof(Tss) >= NUM_CPU,
              "TSS area too small to fit TSSs for all CPUs");
static_assert(SPC_LOCAL_IOP >= TSS_AREA_E, "IO permission bitmap must lie behind TSS area");
static_assert(SPC_LOCAL_IOP_E - TSS_AREA < (1 << 16),
              "TSS and IO permission bitmap must fit in a 64K segment");

Tss& Tss::remote(unsigned id)
{
    assert(id < NUM_CPU);
    return reinterpret_cast<Tss*>(TSS_AREA)[id];
}

Tss& Tss::local() { return remote(Cpu::id()); }

void Tss::setup()
{
    for (mword page = TSS_AREA; page < TSS_AREA_E; page += PAGE_SIZE) {
        mword page_p{Buddy::ptr_to_phys(Buddy::allocator.alloc(0, Buddy::FILL_0))};

        Hpt::boot_hpt().update({page, page_p, Hpt::PTE_NX | Hpt::PTE_G | Hpt::PTE_W | Hpt::PTE_P, PAGE_BITS});
    }
}

void Tss::build()
{
    auto& tss{local()};

    tss.sp0 = reinterpret_cast<mword>(&Cpulocal::get().self);
    tss.iobm = static_cast<uint16>(SPC_LOCAL_IOP - reinterpret_cast<mword>(&tss));
}
