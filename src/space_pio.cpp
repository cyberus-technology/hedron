/*
 * Port I/O Space
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

#include "lock_guard.hpp"
#include "pd.hpp"

Space_pio::Space_pio(Space_mem* mem)
{
    hbmp = Buddy::ptr_to_phys(Buddy::allocator.alloc(1, Buddy::FILL_1));
    gbmp = Buddy::ptr_to_phys(Buddy::allocator.alloc(1, Buddy::FILL_1));

    // This mapping of the IO Permission Bitmap is only used by the CPU to do access control. Map it
    // read-only.
    mem->insert(SPC_LOCAL_IOP, 1, Hpt::PTE_NX | Hpt::PTE_A | Hpt::PTE_P, hbmp);
}

Space_pio::~Space_pio()
{
    Buddy::allocator.free(reinterpret_cast<mword>(Buddy::phys_to_ptr(gbmp)));
    Buddy::allocator.free(reinterpret_cast<mword>(Buddy::phys_to_ptr(hbmp)));
}

Paddr Space_pio::walk(bool host, mword idx)
{
    return (host ? hbmp : gbmp) | (idx_to_virt(idx) & (2 * PAGE_SIZE - 1));
}

void Space_pio::update(bool host, mword idx, mword attr)
{
    mword* m = static_cast<mword*>(Buddy::phys_to_ptr(walk(host, idx)));

    if (attr)
        Atomic::clr_mask(*m, idx_to_mask(idx));
    else
        Atomic::set_mask(*m, idx_to_mask(idx));
}

Tlb_cleanup Space_pio::update(Mdb* mdb, mword r)
{
    assert(this == mdb->space && this != &Pd::kern);

    Lock_guard<Spinlock> guard(mdb->node_lock);

    for (unsigned long i = 0; i < (1UL << mdb->node_order); i++) {
        if (mdb->node_sub & SUBSPACE_HOST) {
            update(true, mdb->node_base + i, mdb->node_attr & ~r);
        }

        if (mdb->node_sub & SUBSPACE_GUEST) {
            update(false, mdb->node_base + i, mdb->node_attr & ~r);
        }
    }

    return {};
}
