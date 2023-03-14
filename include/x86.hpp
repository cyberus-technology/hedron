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
#include "types.hpp"

#define CONCAT2(A, B) A##B
#define CONCAT3(A, B, C) A##B##C

template <typename T> inline void clflush(T* t) { asm volatile("clflush %0" : : "m"(*t) : "memory"); }

NONNULL
inline void* clflush(void* d, size_t n)
{
    for (char* p = static_cast<char*>(d); p < static_cast<char*>(d) + n; p += 32) {
        clflush(p);
    }

    return d;
}

[[noreturn]] inline void shutdown()
{
    for (;;)
        asm volatile("cli; hlt");
}

// Tell the CPU that we are in a busy loop and that it can chill out.
//
// This function is not called pause, because this clashes with a function in unistd.h.
inline void relax() { __builtin_ia32_pause(); }

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

// Write a reader function for a special register.
#define RD_SPECIAL_REG(reg)                                                                                  \
    inline mword CONCAT2(get_, reg)()                                                                        \
    {                                                                                                        \
        mword reg;                                                                                           \
        asm volatile("mov %%" #reg ", %0" : "=r"(reg));                                                      \
        return reg;                                                                                          \
    }

RD_SPECIAL_REG(cr0)
RD_SPECIAL_REG(cr2)
RD_SPECIAL_REG(cr4)

#define WR_SPECIAL_REG(reg)                                                                                  \
    inline void CONCAT2(set_, reg)(mword val) { asm volatile("mov %0, %%" #reg ::"r"(val)); }

WR_SPECIAL_REG(cr0)
WR_SPECIAL_REG(cr2)
WR_SPECIAL_REG(cr4)

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
