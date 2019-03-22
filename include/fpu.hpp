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
        union {
            char data[512];
            struct {
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
        };

        static Slab_cache cache;

    public:
        ALWAYS_INLINE
        inline void save() { asm volatile ("fxsave %0" : "=m" (*data)); }

        ALWAYS_INLINE
        inline void load() { asm volatile ("fxrstor %0" : : "m" (*data)); }

        ALWAYS_INLINE
        static inline void *operator new (size_t) { return cache.alloc(); }

        ALWAYS_INLINE
        static inline void operator delete (void *ptr) { cache.free (ptr); }

        Fpu() {
            // Mask exceptions by default according to SysV ABI spec.
            fcw = 0x37f;
            mxcsr = 0x1f80;
        }
};
