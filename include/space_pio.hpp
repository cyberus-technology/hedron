/*
 * Port I/O Space
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "space.hpp"
#include "tlb_cleanup.hpp"

class Space_mem;

class Space_pio : public Space
{
private:
    Paddr hbmp, gbmp;

    static inline mword idx_to_virt(mword idx)
    {
        return SPC_LOCAL_IOP + (idx / 8 / sizeof(mword)) * sizeof(mword);
    }

    static inline mword idx_to_mask(mword idx) { return 1UL << (idx % (8 * sizeof(mword))); }

    void update(bool, mword, mword);

public:
    /// Construct a new Port I/O space.
    ///
    /// During the construction, this function will modify the page table to setup the IO Permission
    /// Bitmap. See `Tss::build`.
    Space_pio(Space_mem* mem);

    ~Space_pio();

    Paddr walk(bool = false, mword = 0);

    Tlb_cleanup update(Mdb*, mword = 0);
};
