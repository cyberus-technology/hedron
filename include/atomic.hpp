/*
 * Atomic Operations
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "compiler.hpp"

class Atomic
{
public:
    enum Memory_order
    {
        SEQ_CST = __ATOMIC_SEQ_CST,
        ACQUIRE = __ATOMIC_ACQUIRE,
        RELEASE = __ATOMIC_RELEASE,
        RELAXED = __ATOMIC_RELAXED,
    };

    template <typename T, Memory_order O = SEQ_CST> static inline bool cmp_swap(T& ptr, T o, T n)
    {
        return __atomic_compare_exchange_n(&ptr, &o, n, false, O, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline T exchange(T& ptr, T n)
    {
        return __atomic_exchange_n(&ptr, n, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline T load(T& ptr)
    {
        return __atomic_load_n(&ptr, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline void store(T& ptr, T n)
    {
        __atomic_store_n(&ptr, n, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline T add(T& ptr, T v)
    {
        return __atomic_add_fetch(&ptr, v, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline T fetch_add(T& ptr, T v)
    {
        return __atomic_fetch_add(&ptr, v, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline T sub(T& ptr, T v)
    {
        return __atomic_sub_fetch(&ptr, v, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline void set_mask(T& ptr, T v)
    {
        __atomic_fetch_or(&ptr, v, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline void clr_mask(T& ptr, T v)
    {
        __atomic_fetch_and(&ptr, ~v, O);
    }

    template <typename T, Memory_order O = SEQ_CST> static inline bool test_set_bit(T& val, unsigned long bit)
    {
        auto const bitmask{static_cast<T>(1) << bit};
        return __atomic_fetch_or(&val, bitmask, O) & bitmask;
    }
};
