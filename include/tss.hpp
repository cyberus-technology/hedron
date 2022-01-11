/*
 * Task State Segment (TSS)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2018 Thomas Prescher, Cyberus Technology GmbH.
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
#include "selectors.hpp"
#include "types.hpp"

class Tss
{
public:
    uint32 : 32;          // 0x0
    uint64 sp0 PACKED;    // 0x4
    uint64 sp1 PACKED;    // 0xc
    uint64 sp2 PACKED;    // 0x14
    uint64 ist[8] PACKED; // 0x1c
    uint64 : 64 PACKED;

    uint16 trap; // 0x64
    uint16 iobm; // 0x66

    static Tss& local();
    static Tss& remote(unsigned id);

    static void setup();
    static void build();

    static inline void load() { asm volatile("ltr %w0" : : "rm"(SEL_TSS_RUN)); }
};
