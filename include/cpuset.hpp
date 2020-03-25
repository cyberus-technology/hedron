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

#include "atomic.hpp"
#include "types.hpp"

class Cpuset
{
    private:
        mword val;
        static_assert(NUM_CPU <= sizeof(val) * 8, "NUM_CPU is too large");

    public:
        inline explicit Cpuset() : val (0) {}

        inline bool chk (unsigned cpu) const { return val & 1UL << cpu; }

        inline bool set (unsigned cpu) { return !Atomic::test_set_bit (val, cpu); }

        inline void clr (unsigned cpu) { Atomic::clr_mask (val, 1UL << cpu); }

        inline void merge (Cpuset &s) { Atomic::set_mask (val, s.val); }
};
