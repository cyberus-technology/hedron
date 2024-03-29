/*
 * Floating Point Unit (FPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
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

#include "fpu.hpp"
#include "compiler.hpp"
#include "cpu.hpp"
#include "kp.hpp"
#include "x86.hpp"

Fpu::FpuConfig Fpu::config;

static const uint64 required_xsave_state{Cpu::XCR0_X87};
static const uint64 supported_xsave_state{Cpu::XCR0_X87 | Cpu::XCR0_SSE | Cpu::XCR0_AVX |
                                          Cpu::XCR0_AVX512_OP | Cpu::XCR0_AVX512_LO | Cpu::XCR0_AVX512_HI};

static void xsave_enable(uint64 xcr0)
{
    set_cr4(get_cr4() | Cpu::CR4_OSXSAVE);
    set_xcr(0, xcr0);
}

void Fpu::probe()
{
    if (not Cpu::feature(Cpu::FEAT_XSAVE)) {
        panic("Need XSAVE-capable CPU");
    }

    uint32 valid_xcr0_lo, valid_xcr0_hi, current_context, discard;
    uint64 xcr0;

    cpuid(0xD, 0, valid_xcr0_lo, discard, discard, valid_xcr0_hi);

    xcr0 = static_cast<uint64>(valid_xcr0_lo) << 32 | valid_xcr0_lo;
    xcr0 &= supported_xsave_state;
    xsave_enable(xcr0);

    cpuid(0xD, 0, discard, current_context, discard, discard);

    Fpu::config = {xcr0, current_context,
                   Cpu::feature(Cpu::FEAT_XSAVEOPT) ? Fpu::Mode::XSAVEOPT : Fpu::Mode::XSAVE};

    if (Fpu::config.context_size > PAGE_SIZE) {
        panic("Context size is too large for a kernel-page.");
    }
}

void Fpu::init() { xsave_enable(config.xsave_scb); }

Fpu::FpuCtx* Fpu::data()
{
    assert_slow(data_);
    return reinterpret_cast<FpuCtx*>(data_->data_page());
}

void Fpu::save()
{
    uint32 xsave_scb_hi{static_cast<uint32>(config.xsave_scb >> 32)};
    uint32 xsave_scb_lo{static_cast<uint32>(config.xsave_scb)};

    switch (config.mode) {
    case Mode::XSAVEOPT:
        asm volatile("xsaveopt %0" : "=m"(*data()) : "d"(xsave_scb_hi), "a"(xsave_scb_lo) : "memory");
        break;
    case Mode::XSAVE:
        asm volatile("xsave %0" : "=m"(*data()) : "d"(xsave_scb_hi), "a"(xsave_scb_lo) : "memory");
        break;
    }
}

void Fpu::load()
{
    uint32 xsave_scb_hi{static_cast<uint32>(config.xsave_scb >> 32)};
    uint32 xsave_scb_lo{static_cast<uint32>(config.xsave_scb)};

    asm volatile("xrstor %0" : : "m"(*data()), "d"(xsave_scb_hi), "a"(xsave_scb_lo) : "memory");
}

bool Fpu::load_from_user()
{
    uint32 xsave_scb_hi{static_cast<uint32>(config.xsave_scb >> 32)};
    uint32 xsave_scb_lo{static_cast<uint32>(config.xsave_scb)};

    bool skipped{false};
    asm volatile(FIXUP_CALL("xrstor %[xsave_area]")
                 : FIXUP_SKIPPED(skipped)
                 : [xsave_area] "m"(*data()), "d"(xsave_scb_hi), "a"(xsave_scb_lo));

    return not skipped;
}

static bool is_valid_xcr0(uint64 xsave_scb, uint64 xcr0)
{
    mword sanitized{xcr0};

    sanitized &= xsave_scb;
    sanitized |= required_xsave_state;

    if (xcr0 & Cpu::XCR0_AVX) {
        sanitized |= Cpu::XCR0_SSE;
    }

    if (xcr0 & (Cpu::XCR0_AVX512_OP | Cpu::XCR0_AVX512_LO | Cpu::XCR0_AVX512_HI)) {
        sanitized |= Cpu::XCR0_AVX | Cpu::XCR0_AVX512_OP | Cpu::XCR0_AVX512_LO | Cpu::XCR0_AVX512_HI;
    }

    return sanitized == xcr0;
}

bool Fpu::load_xcr0(uint64 xcr0)
{
    if (EXPECT_FALSE(not is_valid_xcr0(config.xsave_scb, xcr0))) {
        return false;
    }

    set_xcr(0, xcr0);
    return true;
}

void Fpu::restore_xcr0() { set_xcr(0, config.xsave_scb); }

Fpu::Fpu(Kp* data_kp) : data_(data_kp)
{
    // Mask exceptions by default according to SysV ABI spec.
    data()->legacy_hdr.fcw = 0x37f;
    data()->legacy_hdr.mxcsr = 0x1f80;
}
