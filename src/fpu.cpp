/*
 * Floating Point Unit (FPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
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

#include "cpu.hpp"
#include "fpu.hpp"
#include "x86.hpp"

Fpu::FpuConfig Fpu::config;
Slab_cache *Fpu::cache;

static const uint64 supported_xsave_state
    {Cpu::XCR0_X87 | Cpu::XCR0_SSE | Cpu::XCR0_AVX | Cpu::XCR0_AVX512_OP | Cpu::XCR0_AVX512_LO | Cpu::XCR0_AVX512_HI};

static void xsave_enable(uint64 xcr0)
{
    set_cr4 (get_cr4() | Cpu::CR4_OSXSAVE);
    set_xcr (0, xcr0);
}

void Fpu::probe()
{
    if (Cpu::feature(Cpu::FEAT_XSAVE)) {
        uint32 valid_xcr0_lo, valid_xcr0_hi, current_context, discard;
        uint64 xcr0;

        Cpu::cpuid (0xD, 0, valid_xcr0_lo, discard, discard, valid_xcr0_hi);

        xcr0 = static_cast<uint64>(valid_xcr0_lo) << 32 | valid_xcr0_lo;
        xcr0 &= supported_xsave_state;
        xsave_enable (xcr0);

        Cpu::cpuid (0xD, 0, discard, current_context, discard, discard);

        Fpu::config = { xcr0, current_context,
                        Cpu::feature (Cpu::FEAT_XSAVEOPT) ? Fpu::Mode::XSAVEOPT : Fpu::Mode::XSAVE };
    } else {
        Fpu::config = { 0, 512, Fpu::Mode::FXSAVE };
    }

    static Slab_cache fpu_cache {Fpu::config.context_size, 64};
    cache = &fpu_cache;
}

void Fpu::init()
{
    if (config.mode != Mode::FXSAVE) {
        xsave_enable(config.xsave_scb);
    }
}

void Fpu::save()
{
    uint32 xsave_scb_hi {static_cast<uint32>(config.xsave_scb >> 32)};
    uint32 xsave_scb_lo {static_cast<uint32>(config.xsave_scb)};

    switch (config.mode) {
    case Mode::XSAVEOPT:
        asm volatile ("xsaveopt %0" : "=m" (*data) : "d" (xsave_scb_hi), "a" (xsave_scb_lo) : "memory");
        break;
    case Mode::XSAVE:
        asm volatile ("xsave %0" : "=m" (*data) : "d" (xsave_scb_hi), "a" (xsave_scb_lo) : "memory");
        break;
    case Mode::FXSAVE:
        asm volatile ("fxsave %0" : "=m" (*data));
        break;
    }
}

void Fpu::load()
{
    uint32 xsave_scb_hi {static_cast<uint32>(config.xsave_scb >> 32)};
    uint32 xsave_scb_lo {static_cast<uint32>(config.xsave_scb)};

    switch (config.mode) {
    case Mode::XSAVEOPT:
    case Mode::XSAVE:
        asm volatile ("xrstor %0" : : "m" (*data), "d" (xsave_scb_hi), "a" (xsave_scb_lo) : "memory");
        break;
    case Mode::FXSAVE:
        asm volatile ("fxrstor %0" : : "m" (*data));
        break;
    }
}

Fpu::Fpu() : data (static_cast<FpuCtx *>(cache->alloc())) {
    // Mask exceptions by default according to SysV ABI spec.
    data->legacy_hdr.fcw = 0x37f;
    data->legacy_hdr.mxcsr = 0x1f80;
}
