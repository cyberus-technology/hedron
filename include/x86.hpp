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
 * This file is part of the Hedron microhypervisor.
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
#include "types.hpp"

template <typename T> inline void clflush(T* t) { asm volatile("clflush %0" : : "m"(*t) : "memory"); }

NONNULL
inline void* clflush(void* d, size_t n)
{
    for (char* p = static_cast<char*>(d); p < static_cast<char*>(d) + n; p += 32) {
        clflush(p);
    }

    return d;
}

NORETURN
inline void shutdown()
{
    for (;;)
        asm volatile("cli; hlt");
}

inline void pause() { asm volatile("pause" : : : "memory"); }

inline uint64 rdtsc()
{
    mword h, l;
    asm volatile("rdtsc" : "=a"(l), "=d"(h));
    return static_cast<uint64>(h) << 32 | l;
}

inline void cpuid(unsigned leaf, unsigned subleaf, uint32& eax, uint32& ebx, uint32& ecx, uint32& edx)
{
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf), "c"(subleaf));
}

inline void cpuid(unsigned leaf, uint32& eax, uint32& ebx, uint32& ecx, uint32& edx)
{
    cpuid(leaf, 0, eax, ebx, ecx, edx);
}

inline void wbinvd() { asm volatile("wbinvd" ::: "memory"); }

inline mword get_cr0()
{
    mword cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

inline void set_cr0(mword cr0) { asm volatile("mov %0, %%cr0" : : "r"(cr0)); }

inline mword get_cr2()
{
    mword cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

inline void set_cr2(mword cr2) { asm volatile("mov %0, %%cr2" : : "r"(cr2)); }

inline mword get_cr4()
{
    mword cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

inline void set_cr4(mword cr4) { asm volatile("mov %0, %%cr4" : : "r"(cr4)); }

inline mword get_xcr(uint32 n)
{
    mword h{0}, l{0};
    asm volatile("xgetbv" : "=a"(l), "=d"(h) : "c"(n));
    return static_cast<uint64>(h) << 32 | l;
}

inline void set_xcr(uint32 n, mword val)
{
    asm volatile("xsetbv" ::"c"(n), "a"(static_cast<uint32>(val)), "d"(val >> 32));
}

inline void swapgs() { asm volatile("swapgs"); }

#define CONCAT3(A, B, C) A##B##C

#define WR_SEGMENT_BASE(seg)                                                                                 \
    inline void CONCAT3(wr, seg, base)(uint64 value) { asm volatile("wr" #seg "base %0" ::"r"(value)); }

#define RD_SEGMENT_BASE(seg)                                                                                 \
    inline uint64 CONCAT3(rd, seg, base)()                                                                   \
    {                                                                                                        \
        uint64 value;                                                                                        \
        asm volatile("rd" #seg "base %0" : "=r"(value));                                                     \
        return value;                                                                                        \
    }

WR_SEGMENT_BASE(fs)
RD_SEGMENT_BASE(fs)

WR_SEGMENT_BASE(gs)
RD_SEGMENT_BASE(gs)
