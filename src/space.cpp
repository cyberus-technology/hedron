/*
 * Generic Space
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

#include "space.hpp"
#include "lock_guard.hpp"
#include "math.hpp"
#include "mdb.hpp"

Mdb* Space::tree_lookup(mword idx, bool next)
{
    Lock_guard<Spinlock> guard(lock);
    return Mdb::lookup(tree, idx, next);
}

bool Space::tree_insert(Mdb* node)
{
    Lock_guard<Spinlock> guard(node->space->lock);
    return Mdb::insert<Mdb>(&node->space->tree, node);
}

bool Space::tree_remove(Mdb* node)
{
    Lock_guard<Spinlock> guard(node->space->lock);
    return Mdb::remove<Mdb>(&node->space->tree, node);
}

void Space::addreg(mword addr, size_t size, mword attr, mword type)
{
    Lock_guard<Spinlock> guard(lock);

    for (mword o; size; size -= 1UL << o, addr += 1UL << o)
        Mdb::insert<Mdb>(&tree, new Mdb(nullptr, addr, addr, (o = max_order(addr, size)), attr, type));
}
