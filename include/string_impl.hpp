/*
 * String Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include <stddef.h>

inline void* impl_memcpy(void* d, void const* s, size_t n)
{
    void* dummy;
    asm volatile("rep; movsb" : "=D"(dummy), "+S"(s), "+c"(n) : "0"(d) : "memory");
    return d;
}

inline void* impl_memmove(void* d, void const* s, size_t n)
{
    if (d < s) {
        return impl_memcpy(d, s, n);
    } else {
        char* d_end = static_cast<char*>(d) + n - 1;
        char const* s_end = static_cast<char const*>(s) + n - 1;

        asm volatile("std; rep movsb; cld" : "+S"(s_end), "+D"(d_end), "+c"(n) : : "memory");

        return d;
    }
}

inline void* impl_memset(void* d, int c, size_t n)
{
    void* dummy;
    asm volatile("rep; stosb" : "=D"(dummy), "+c"(n) : "0"(d), "a"(c) : "memory");
    return d;
}

inline bool impl_strnmatch(char const* s1, char const* s2, size_t n)
{
    while (n && *s1 == *s2)
        s1++, s2++, n--;

    return n == 0;
}
