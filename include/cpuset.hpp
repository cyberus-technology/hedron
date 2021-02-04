/*
 * CPU Set
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

#pragma once

#include "bitmap.hpp"
#include "types.hpp"

class Cpuset
{
    private:
        Bitmap<mword, NUM_CPU> bits {false};

    public:
        bool chk (unsigned cpu) const { return bits.atomic_fetch(cpu); }

        bool set (unsigned cpu) { return bits[cpu].atomic_fetch_set(); }

        void clr (unsigned cpu) { bits[cpu].atomic_clear(); }

        void merge (Cpuset &s) { bits.atomic_union(s.bits); }
};
