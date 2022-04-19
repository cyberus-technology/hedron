/*
 * Policy implementations for page table code
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "buddy.hpp"
#include "types.hpp"

template <typename T = mword> class Page_alloc_policy
{
public:
    using entry = T;
    using pointer = T*;

    static pointer phys_to_pointer(entry e) { return static_cast<pointer>(Buddy::phys_to_ptr(e)); }
    static entry pointer_to_phys(pointer p) { return Buddy::ptr_to_phys(p); }

    static pointer alloc_zeroed_page()
    {
        return static_cast<pointer>(Buddy::allocator.alloc(0, Buddy::FILL_0));
    }
    static void free_page(pointer ptr) { Buddy::allocator.free(reinterpret_cast<mword>(ptr)); }
};
