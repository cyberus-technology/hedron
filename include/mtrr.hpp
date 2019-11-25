/*
 * Memory Type Range Registers (MTRR)
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

#pragma once

#include "math.hpp"
#include "static_vector.hpp"

class Mtrr
{
    public:
        uint64 const base;
        uint64 const mask;

        uint64 size() const
        {
            return 1ULL << (static_cast<mword>(mask) ? bit_scan_forward (static_cast<mword>(mask >> 12)) + 12 :
                                                       bit_scan_forward (static_cast<mword>(mask >> 32)) + 32);
        }

    public:
        Mtrr (uint64 b, uint64 m) : base (b), mask (m) {}
};

class Mtrr_state
{
    private:

        static constexpr size_t MAX_VMTRR {16};
        Static_vector<Mtrr, MAX_VMTRR> vmtrr;

        // The default memory type.
        unsigned dtype;

    public:
        INIT void init();
        INIT unsigned memtype (uint64 paddr, uint64 &next);

        // Return a singleton instance.
        static Mtrr_state &get();
};
