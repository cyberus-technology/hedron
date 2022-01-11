/*
 * Floating Point Unit (FPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#pragma once

#include "slab.hpp"

class Fpu
{
private:
    struct FxsaveHdr {
        uint16 fcw;
        uint16 fsw;
        uint8 ftw;
        uint8 res_;
        uint16 fop;
        uint64 fip;
        uint64 fdp;
        uint32 mxcsr;
        uint32 mxcsr_mask;
    };

    struct FpuCtx {
        FxsaveHdr legacy_hdr;
    };

    FpuCtx* data;

    static Slab_cache* cache;

    enum class Mode : uint8
    {
        XSAVEOPT,
        XSAVE,
    };

    struct FpuConfig {
        uint64 xsave_scb; // State-Component Bitmap
        size_t context_size;
        Mode mode;
    };

    static FpuConfig config;

public:
    static void probe();
    static void init();

    void save();
    void load();

    static bool load_xcr0(uint64 xcr0);
    static void restore_xcr0();

    Fpu();
    ~Fpu() { cache->free(data); }
};
