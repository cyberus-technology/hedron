/*
 * Intel VT Extended page table (EPT) modification
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

#pragma once

#include "generic_page_table.hpp"
#include "page_table_policies.hpp"
#include "tlb_cleanup.hpp"

class Ept_new;
using Ept_page_table = Generic_page_table<9, mword, Atomic_access_policy<>, No_clflush_policy,
                                          Page_alloc_policy<>, Tlb_cleanup, Ept_new>;

class Ept_new : public Ept_page_table
{
    private:

        // The number of leaf levels we support. This is adjusted by
        // set_supported_leaf_levels.
        static level_t supported_leaf_levels;

        // EPT invalidation types
        enum : mword {
            INVEPT_SINGLE_CONTEXT = 1,
        };

        // EPTP constants
        enum {
            EPTP_WB = 6,
            EPTP_WALK_LENGTH_SHIFT = 3,
        };

    public:
        enum : pte_t {
            PTE_R = 1UL << 0,
            PTE_W = 1UL << 1,
            PTE_X = 1UL << 2,

            PTE_P = PTE_R | PTE_W | PTE_X,

            PTE_I = 1UL << 6,
            PTE_S = 1UL << 7,
        };

        static constexpr pte_t mask {0xFFF};
        static constexpr pte_t all_rights {PTE_R | PTE_W | PTE_X};

        // Adjust the number of leaf levels to the given value.
        static void set_supported_leaf_levels(level_t level);

        // Create a page table from scratch.
        Ept_new() : Ept_page_table(4, supported_leaf_levels) {}

        // Convert mapping database attributes to page table attributes.
        static pte_t hw_attr(mword a, mword mtrr_type);

        // Do a single-context invalidation for this EPT.
        void flush()
        {
            struct { uint64 eptp, rsvd; } const desc { vmcs_eptp(), 0 };
            static_assert(sizeof(desc) == 16, "INVEPT descriptor layout is broken");

            bool ret;
            asm volatile ("invept %1, %2; seta %0" : "=q" (ret) : "m" (desc), "r" (INVEPT_SINGLE_CONTEXT) : "cc", "memory");
            assert (ret);
        }

        // Return a VMCS EPT pointer to this EPT.
        uint64 vmcs_eptp() const
        {
            return static_cast<uint64>(root())
                | (max_levels() - 1) << EPTP_WALK_LENGTH_SHIFT | EPTP_WB;
        }
};
