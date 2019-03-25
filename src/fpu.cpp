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

#include "fpu.hpp"

Fpu::FpuConfig Fpu::config;
Slab_cache *Fpu::cache;

void Fpu::probe()
{
    Fpu::config = { 512, Fpu::Mode::FXSAVE };

    static Slab_cache fpu_cache {Fpu::config.context_size, 64};
    cache = &fpu_cache;
}

void Fpu::init()
{
}

void Fpu::save()
{
    switch (config.mode) {
    case Mode::FXSAVE:
        asm volatile ("fxsave %0" : "=m" (*data));
        break;
    }
}

void Fpu::load()
{
    switch (config.mode) {
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
