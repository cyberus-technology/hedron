/*
 * Floating Point Unit (FPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "memory.hpp"
#include "refptr.hpp"
#include "types.hpp"

class Kp;

class Fpu
{
private:
    static constexpr size_t FXSAVE_HEADER_SIZE{32ul}; // Intel SDM Vol. 1 Chap. 10.5.1

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
    static_assert(sizeof(FxsaveHdr) == FXSAVE_HEADER_SIZE);
    static constexpr size_t FXSAVE_AREA_SIZE{512ul}; // Intel SDM Vol. 1 Chap. 10.5.1

    struct FxsaveData {
        uint8 fpu_data[FXSAVE_AREA_SIZE - FXSAVE_HEADER_SIZE];
    };

    struct FpuCtx {
        FxsaveHdr legacy_hdr;
        FxsaveData legacy_data;
    };
    static_assert(sizeof(FpuCtx) <= PAGE_SIZE, "FpuCtx has to fit into a kernel page.");

    Refptr<Kp> data_;
    FpuCtx* data();

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

    explicit Fpu(Kp* data_kp);
    ~Fpu() = default;
};
