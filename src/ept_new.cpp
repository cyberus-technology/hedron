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

#include "ept_new.hpp"
#include "mdb.hpp"

Ept_new::level_t Ept_new::supported_leaf_levels {1};

Ept_new::pte_t Ept_new::hw_attr(mword a, mword mtrr_type)
{
    auto const none {static_cast<decltype(Ept_new::PTE_R)>(0)};

    if (a) {
        return mtrr_type << 3
            | (a & Mdb::MEM_R ? Ept_new::PTE_R : none)
            | (a & Mdb::MEM_W ? Ept_new::PTE_W : none)
            | (a & Mdb::MEM_X ? Ept_new::PTE_X : none);
    }

    return 0;
}

void Ept_new::set_supported_leaf_levels(Ept_new::level_t level)
{
    assert (level > 0);
    supported_leaf_levels = level;
}
