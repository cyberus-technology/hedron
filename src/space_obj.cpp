/*
 * Object Space
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

#include "lock_guard.hpp"
#include "pd.hpp"

Space_mem* Space_obj::space_mem() { return static_cast<Pd*>(this); }

Paddr Space_obj::walk(mword idx, bool& shootdown)
{
    Paddr const frame_0 = Buddy::ptr_to_phys(&PAGE_0);
    mword virt = idx_to_virt(idx);
    Paddr phys;
    void* ptr;

    if (!space_mem()->lookup(virt, &phys) || (phys & ~PAGE_MASK) == frame_0) {
        shootdown = (phys & ~PAGE_MASK) == frame_0;

        Paddr p = Buddy::ptr_to_phys(ptr = Buddy::allocator.alloc(0, Buddy::FILL_0));

        if ((phys = space_mem()->replace(virt, p | Hpt::PTE_NX | Hpt::PTE_D | Hpt::PTE_A | Hpt::PTE_W |
                                                   Hpt::PTE_P)) != p)
            Buddy::allocator.free(reinterpret_cast<mword>(ptr));

        phys |= virt & PAGE_MASK;
    }

    return phys;
}

Tlb_cleanup Space_obj::update(mword idx, Capability cap)
{
    bool shootdown = false;
    *static_cast<Capability*>(Buddy::phys_to_ptr(walk(idx, shootdown))) = cap;
    return Tlb_cleanup{shootdown};
}

size_t Space_obj::lookup(mword idx, Capability& cap)
{
    Paddr phys;
    if (!space_mem()->lookup(idx_to_virt(idx), &phys) || (phys & ~PAGE_MASK) == Buddy::ptr_to_phys(&PAGE_0))
        return 0;

    cap = *static_cast<Capability*>(Buddy::phys_to_ptr(phys));

    return 1;
}

Tlb_cleanup Space_obj::update(Mdb* mdb, mword r)
{
    assert(this == mdb->space && this != &Pd::kern);
    Lock_guard<Spinlock> guard(mdb->node_lock);
    return update(mdb->node_base,
                  Capability(reinterpret_cast<Kobject*>(mdb->node_phys), mdb->node_attr & ~r));
}

bool Space_obj::insert_root(Kobject* obj)
{
    if (!obj->space->tree_insert(obj))
        return false;

    if (obj->space != static_cast<Space_obj*>(&Pd::kern))
        static_cast<Space_obj*>(obj->space)->update(obj->node_base, Capability(obj, obj->node_attr));

    return true;
}

void Space_obj::page_fault(mword addr, mword error)
{
    assert(!(error & Hpt::ERR_W));
    Pd::current()->Space_mem::replace(addr,
                                      Buddy::ptr_to_phys(&PAGE_0) | Hpt::PTE_NX | Hpt::PTE_A | Hpt::PTE_P);
}
