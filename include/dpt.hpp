/*
 * Intel IOMMU Device Page Table (DPT) modification
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
#include "hpt.hpp"
#include "page_table_policies.hpp"
#include "tlb_cleanup.hpp"

class Dpt;
using Dpt_page_table = Generic_page_table<9, mword, Atomic_access_policy<>, Clflush_policy,
                                          Page_alloc_policy<>, Tlb_cleanup, Dpt>;

class Dpt : public Dpt_page_table
{
    private:

        // The number of leaf levels we support. This is adjusted by
        // set_supported_leaf_levels.
        static level_t supported_leaf_levels;

    public:
        enum : pte_t {
            PTE_R = 1UL << 0,
            PTE_W = 1UL << 1,

            PTE_S = 1UL << 7,
            PTE_P = PTE_R | PTE_W,
        };

        static constexpr pte_t mask {PTE_R | PTE_W};
        static constexpr pte_t all_rights {PTE_R | PTE_W};

        // Adjust the number of leaf levels.
        //
        // This function can be called multiple times and the minimum of all
        // calls will be used.
        static void lower_supported_leaf_levels(level_t level);

        // Return the root pointer as if the page table had only the given
        // number of levels.
        //
        // Calling root (max_levels()) is equivalent to root(), just less
        // efficient.
        phys_t root (level_t level)
        {
            assert (level > 0 and level <= max_levels());

            Tlb_cleanup cleanup;
            pte_pointer_t root {walk_down_and_split (cleanup, 0, level - 1, true)};

            assert (root != nullptr and not cleanup.need_tlb_flush());
            return Buddy::ptr_to_phys (root);
        }

        // Create a page table from scratch.
        Dpt() : Dpt_page_table(4, supported_leaf_levels < 0 ? 1 : supported_leaf_levels) {}

        // Convert a HPT mapping into a mapping for the DPT.
        static Mapping convert_mapping(Hpt::Mapping const &hpt_mapping);
};
