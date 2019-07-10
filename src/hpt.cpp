/*
 * Host Page Table (HPT)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
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

#include "bits.hpp"
#include "hpt.hpp"

// Perform a recursive page table copy. No page table structures are shared and
// a full set of new page tables are allocated.
Hpt Hpt::copy (unsigned lvl) const
{
    size_t pt_entries {1UL << bits_per_level()};

    // Copy terminal entries. The root entry has no present bit.
    if ((lvl + 1 == max()) or (lvl != 0 and not present()) or super()) {
        return *this;
    }

    // Recurse for entries that reference further tables.
    Hpt *dst_pt {new Hpt};
    auto *src_pt {static_cast<Hpt const *>(Buddy::phys_to_ptr (addr()))};

    for (size_t i = 0; i < pt_entries; i++) {
        // The low half of the address space contains kernel bootstrap code
        // and should never be copied to new page tables.
        bool user_mapping {lvl == 0 and i < (pt_entries / 2)};

        dst_pt[i] = user_mapping ? Hpt {} : src_pt[i].copy (lvl + 1);
    }

    Hpt copied;

    copied.val = Buddy::ptr_to_phys (dst_pt) | attr();
    return copied;
}

Paddr Hpt::replace (mword v, mword p)
{
    Hpt o, *e = walk (v, 0); assert (e);

    do o = *e; while (o.val != p && !(o.attr() & HPT_W) && !e->set (o.val, p));

    return e->addr();
}

void *Hpt::remap (Paddr phys)
{
    Hptp hpt (current());

    size_t size = 1UL << (bits_per_level() + PAGE_BITS);

    mword offset = phys & (size - 1);

    phys &= ~offset;

    Paddr old; mword attr;
    if (hpt.lookup (SPC_LOCAL_REMAP, old, attr)) {
        hpt.update (SPC_LOCAL_REMAP,        bits_per_level(), 0, 0, Hpt::TYPE_DN); flush (SPC_LOCAL_REMAP);
        hpt.update (SPC_LOCAL_REMAP + size, bits_per_level(), 0, 0, Hpt::TYPE_DN); flush (SPC_LOCAL_REMAP + size);
    }

    hpt.update (SPC_LOCAL_REMAP,        bits_per_level(), phys,        HPT_W | HPT_P);
    hpt.update (SPC_LOCAL_REMAP + size, bits_per_level(), phys + size, HPT_W | HPT_P);

    return reinterpret_cast<void *>(SPC_LOCAL_REMAP + offset);
}
