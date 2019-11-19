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

#include "dpt_new.hpp"
#include "mdb.hpp"

Dpt_new::level_t Dpt_new::supported_leaf_levels {-1};

Dpt_new::pte_t Dpt_new::hw_attr(mword a)
{
    auto const none {static_cast<decltype(Dpt_new::PTE_R)>(0)};

    if (a) {
        return
              (a & Mdb::MEM_R ? Dpt_new::PTE_R : none)
            | (a & Mdb::MEM_W ? Dpt_new::PTE_W : none);
    }

    return 0;
}

void Dpt_new::lower_supported_leaf_levels(Dpt_new::level_t level)
{
    assert (level > 0);

    supported_leaf_levels = supported_leaf_levels < 0 ? level : min(supported_leaf_levels, level);
}
