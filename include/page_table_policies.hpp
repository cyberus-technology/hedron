/*
 * Policy implementations for page table code
 *
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

#include "atomic.hpp"
#include "buddy.hpp"
#include "types.hpp"
#include "x86.hpp"

template <typename T = mword> class Atomic_access_policy
{
public:
    using entry = T;
    using pointer = T*;

    static entry read(pointer ptr) { return Atomic::load(*ptr); }
    static void write(pointer ptr, entry e) { Atomic::store(*ptr, e); }

    static bool cmp_swap(pointer ptr, entry old, entry desired)
    {
        return Atomic::cmp_swap(*ptr, old, desired);
    }
    static entry exchange(pointer ptr, entry desired) { return Atomic::exchange(*ptr, desired); }
};
