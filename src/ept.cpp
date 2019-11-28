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

#include "ept.hpp"
#include "hpt.hpp"
#include "mdb.hpp"

Ept::level_t Ept::supported_leaf_levels {1};

static Ept::pte_t attr_from_hpt(mword a)
{
    auto const none {static_cast<decltype(Ept::PTE_R)>(0)};
    Ept::pte_t const mt {((a >> Hpt::PTE_MT_SHIFT) & Hpt::MT_MASK) << Ept::PTE_MT_SHIFT};

    if (a & Hpt::PTE_P) {
        // Only user accessible and delegatable mappings should ever end up in the
        // EPT.
        assert ((a & Hpt::PTE_U)       != 0);
        assert ((a & Hpt::PTE_NODELEG) == 0);

        return mt | Ept::PTE_R
            | (a & Hpt::PTE_W ? Ept::PTE_W : none)
            | (a & Hpt::PTE_NX ? none : Ept::PTE_X);
    }

    return none;
}

Ept::Mapping Ept::convert_mapping(Hpt::Mapping const &hpt_mapping)
{
    return {hpt_mapping.vaddr, hpt_mapping.paddr, attr_from_hpt (hpt_mapping.attr), hpt_mapping.order};
}

void Ept::set_supported_leaf_levels(Ept::level_t level)
{
    assert (level > 0);
    supported_leaf_levels = level;
}
