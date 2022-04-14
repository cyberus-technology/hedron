/*
 * Generic implementation of Memory Type Range Registers (MTRR)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "math.hpp"
#include "memory.hpp"
#include "static_vector.hpp"

// A single variable-range MTRR.
//
// These MTRRs match memory ranges by applying the mask to the address and
// checking whether the result is equal to base. If this is the case, the
// particular variable-range MTRR matches and the stored memory type applies.
class Mtrr
{
public:
    uint64 const base;
    uint64 const mask;

    enum : uint64
    {
        MTRR_MASK_VALID = 0x800U,
    };

    bool valid() const { return mask & MTRR_MASK_VALID; }

    // Returns the size of the MTRR region in bytes.
    uint64 size() const
    {
        static_assert(sizeof(uint64) == sizeof(mword), "bit_scan_forward argument width mismatch");
        return 1ULL << (bit_scan_forward(static_cast<mword>(mask >> 12)) + 12);
    }

    Mtrr(uint64 b, uint64 m) : base(b), mask(m) {}
};

template <typename MSR> class Generic_mtrr_state
{
private:
    static constexpr size_t MAX_VAR_MTRR{16};
    Static_vector<Mtrr, MAX_VAR_MTRR> var_mtrr;

    unsigned default_type;

    uint64 read(size_t index) { return MSR::read(typename MSR::Register(index)); }

public:
    void init()
    {
        size_t const count{read(MSR::IA32_MTRR_CAP) & 0xff};

        default_type = read(MSR::IA32_MTRR_DEF_TYPE) & 0xff;

        for (size_t i = 0; i < count; i++) {
            Mtrr const mtrr{read(MSR::IA32_MTRR_PHYS_BASE + 2 * i), read(MSR::IA32_MTRR_PHYS_MASK + 2 * i)};

            if (mtrr.valid()) {
                var_mtrr.emplace_back(mtrr);
            }
        }
    }

    unsigned memtype(uint64 phys, uint64& next)
    {
        if (phys < 0x80000) {
            next = 1 + (phys | 0xffff);
            return static_cast<unsigned>(read(MSR::IA32_MTRR_FIX64K_BASE) >> (phys >> 13 & 0x38)) & 0xff;
        }

        if (phys < 0xc0000) {
            next = 1 + (phys | 0x3fff);
            return static_cast<unsigned>(read(MSR::IA32_MTRR_FIX16K_BASE + (phys >> 17 & 0x1)) >>
                                         (phys >> 11 & 0x38)) &
                   0xff;
        }

        if (phys < 0x100000) {
            next = 1 + (phys | 0xfff);
            return static_cast<unsigned>(read(MSR::IA32_MTRR_FIX4K_BASE + (phys >> 15 & 0x7)) >>
                                         (phys >> 9 & 0x38)) &
                   0xff;
        }

        unsigned type = ~0U;
        next = ~0ULL;

        for (Mtrr& mtrr : var_mtrr) {
            uint64 base = mtrr.base & ~PAGE_MASK;

            if (phys < base)
                next = min(next, base);

            else if (((phys ^ mtrr.base) & mtrr.mask) >> PAGE_BITS == 0) {
                next = min(next, base + mtrr.size());
                type = min(type, static_cast<unsigned>(mtrr.base) & 0xff);
            }
        }

        return type == ~0U ? default_type : type;
    }
};
