/*
 * Portal
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
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

#include "kobject.hpp"
#include "mtd.hpp"

class Ec;

class Pt : public Typed_kobject<Kobject::Type::PT>
{
private:
    static Slab_cache cache;

    static void free(Rcu_elem* a) { delete static_cast<Pt*>(a); }

public:
    // Capability permission bitmask.
    enum
    {
        PERM_CTRL = 1U << 0,
        PERM_CALL = 1U << 1,
    };

    Refptr<Ec> const ec;
    Mtd const mtd;
    mword const ip;
    mword id;

    Pt(Pd*, mword, Ec*, Mtd, mword);

    inline void set_id(mword i) { id = i; }

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
