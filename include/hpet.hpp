/*
 * High Precision Event Timer (HPET)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Florian Pester, Cyberus Technology GmbH.
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

#pragma once

#include "list.hpp"
#include "slab.hpp"

class Hpet : public Forward_list<Hpet>
{
    friend class Hip;

    private:
        Paddr    const      phys;
        unsigned const      id;
        uint16              rid;

        static Hpet *       list;
        static Slab_cache   cache;

    public:
        explicit inline Hpet (Paddr p, unsigned i) : Forward_list<Hpet> (list), phys (p), id (i), rid (0) {}

        static inline void *operator new (size_t) { return cache.alloc(); }

        static inline bool claim_dev (unsigned r, unsigned i)
        {
            for (Hpet *hpet = list; hpet; hpet = hpet->next)
                if (hpet->rid == 0 && hpet->id == i) {
                    hpet->rid = static_cast<uint16>(r);
                    return true;
                }

            return false;
        }

        static inline unsigned phys_to_rid (Paddr p)
        {
            for (Hpet *hpet = list; hpet; hpet = hpet->next)
                if (hpet->phys == p)
                    return hpet->rid;

            return ~0U;
        }
};
