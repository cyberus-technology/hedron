/*
 * Math Helper Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "compiler.hpp"
#include "types.hpp"

template <typename T>
constexpr T min (T v1, T v2)
{
    return v1 < v2 ? v1 : v2;
}

template <typename T>
constexpr T max (T v1, T v2)
{
    return v1 > v2 ? v1 : v2;
}

constexpr inline long int bit_scan_reverse (mword val)
{
    if (EXPECT_FALSE (!val))
        return -1;

    static_assert(sizeof(mword) == sizeof(long long), "builtin call has wrong size");
#ifdef __clang__
    return sizeof(long long)*8 - __builtin_clzll(val) - 1;
#else
    return __builtin_ia32_bsrdi(val);
#endif
}

constexpr inline long int bit_scan_forward (mword val)
{
    if (EXPECT_FALSE (!val))
        return -1;

    static_assert(sizeof(mword) == sizeof(long), "builtin call has wrong size");
    return __builtin_ctzl(val);
}

constexpr inline unsigned long max_order (mword base, size_t size)
{
    long int o = bit_scan_reverse (size);

    if (base)
        o = min (bit_scan_forward (base), o);

    return o;
}

constexpr inline mword align_dn (mword val, mword align)
{
    val &= ~(align - 1);                // Expect power-of-2
    return val;
}

constexpr inline mword align_up (mword val, mword align)
{
    val += (align - 1);                 // Expect power-of-2
    return align_dn (val, align);
}
