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

#include "dpt.hpp"
#include "hpt.hpp"
#include "mdb.hpp"

Dpt::level_t Dpt::supported_leaf_levels {-1};

static Dpt::pte_t attr_from_hpt(mword a)
{
    auto const none {static_cast<decltype(Dpt::PTE_R)>(0)};

    if (a & Hpt::PTE_P) {
        // Only user accessible and delegatable mappings should ever end up in
        // the DPT.
        assert ((a & Hpt::PTE_U)       != 0);
        assert ((a & Hpt::PTE_NODELEG) == 0);

        return Dpt::PTE_R | (a & Hpt::PTE_W ? Dpt::PTE_W : none);
    }

    return none;
}

Dpt::Mapping Dpt::convert_mapping(Hpt::Mapping const &hpt_mapping)
{
    return {hpt_mapping.vaddr, hpt_mapping.paddr, attr_from_hpt (hpt_mapping.attr), hpt_mapping.order};
}

void Dpt::lower_supported_leaf_levels(Dpt::level_t level)
{
    assert (level > 0);

    supported_leaf_levels = supported_leaf_levels < 0 ? level : min(supported_leaf_levels, level);
}
