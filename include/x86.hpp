/*
 * x86-Specific Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
 * Copyright (C) 2019 Markus Partheymüller, Cyberus Technology GmbH.
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

template <typename T>
static inline void flush (T t)
{
    asm volatile ("clflush %0" : : "m" (*t) : "memory");
}

NONNULL
inline void *flush (void *d, size_t n)
{
    for (char *p = static_cast<char *>(d); p < static_cast<char *>(d) + n; p += 32)
        flush (p);

    return d;
}

NORETURN
inline void shutdown()
{
    for (;;)
        asm volatile ("cli; hlt");
}

static inline void wbinvd()
{
    asm volatile ("wbinvd" : : : "memory");
}

static inline void pause()
{
    asm volatile ("pause" : : : "memory");
}

static inline uint64 rdtsc()
{
    mword h, l;
    asm volatile ("rdtsc" : "=a" (l), "=d" (h));
    return static_cast<uint64>(h) << 32 | l;
}

static inline void cpuid (unsigned leaf, unsigned subleaf, uint32 &eax, uint32 &ebx, uint32 &ecx, uint32 &edx)
{
    asm volatile ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (leaf), "c" (subleaf));
}

static inline void cpuid (unsigned leaf, uint32 &eax, uint32 &ebx, uint32 &ecx, uint32 &edx)
{
    cpuid (leaf, 0, eax, ebx, ecx, edx);
}

static inline mword get_cr0()
{
    mword cr0;
    asm volatile ("mov %%cr0, %0" : "=r" (cr0));
    return cr0;
}

static inline void set_cr0 (mword cr0)
{
    asm volatile ("mov %0, %%cr0" : : "r" (cr0));
}

static inline mword get_cr2()
{
    mword cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2));
    return cr2;
}

static inline void set_cr2 (mword cr2)
{
    asm volatile ("mov %0, %%cr2" : : "r" (cr2));
}

static inline mword get_cr4()
{
    mword cr4;
    asm volatile ("mov %%cr4, %0" : "=r" (cr4));
    return cr4;
}

static inline void set_cr4 (mword cr4)
{
    asm volatile ("mov %0, %%cr4" : : "r" (cr4));
}

static inline mword get_xcr(uint32 n)
{
    mword h {0}, l {0};
    asm volatile ("xgetbv" : "=a" (l), "=d" (h) : "c"(n));
    return static_cast<uint64>(h) << 32 | l;
}

static inline void set_xcr(uint32 n, mword val)
{
    asm volatile ("xsetbv" :: "c" (n), "a" (static_cast<uint32>(val)), "d" (val >> 32));
}
