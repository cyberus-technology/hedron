/*
 * Central Processing Unit (CPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2015 Alexander Boettcher, Genode Labs GmbH
 *
 * Copyright (C) 2017-2019 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
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

#include "compiler.hpp"
#include "config.hpp"
#include "cpuinfo.hpp"
#include "types.hpp"
#include "assert.hpp"
#include "cpulocal.hpp"

class Cpu
{
    private:
        static char const * const vendor_string[];

        ALWAYS_INLINE
        static inline Cpu_info check_features();

        ALWAYS_INLINE
        static inline void setup_thermal();

    public:
        enum Feature
        {
            FEAT_MCE            =  7,
            FEAT_SEP            = 11,
            FEAT_MCA            = 14,
            FEAT_ACPI           = 22,
            FEAT_HTT            = 28,
            FEAT_VMX            = 37,
            FEAT_PCID           = 49,
            FEAT_TSC_DEADLINE   = 56,
            FEAT_XSAVE          = 58,
            FEAT_SMEP           = 103,
            FEAT_SMAP           = 116,
            FEAT_1GB_PAGES      = 154,
            FEAT_CMP_LEGACY     = 161,
            FEAT_SVM            = 162,
            FEAT_XSAVEOPT       = 192,
        };

        enum
        {
            EXC_DB          = 1,
            EXC_NM          = 7,
            EXC_TS          = 10,
            EXC_GP          = 13,
            EXC_PF          = 14,
            EXC_AC          = 17,
            EXC_MC          = 18,
        };

        enum
        {
            CR0_PE          = 1ul << 0,         // 0x1
            CR0_MP          = 1ul << 1,         // 0x2
            CR0_EM          = 1ul << 2,         // 0x4
            CR0_TS          = 1ul << 3,         // 0x8
            CR0_ET          = 1ul << 4,         // 0x10
            CR0_NE          = 1ul << 5,         // 0x20
            CR0_WP          = 1ul << 16,        // 0x10000
            CR0_AM          = 1ul << 18,        // 0x40000
            CR0_NW          = 1ul << 29,        // 0x20000000
            CR0_CD          = 1ul << 30,        // 0x40000000
            CR0_PG          = 1ul << 31         // 0x80000000
        };

        enum
        {
            CR4_DE          = 1UL << 3,         // 0x8
            CR4_PSE         = 1UL << 4,         // 0x10
            CR4_PAE         = 1UL << 5,         // 0x20
            CR4_MCE         = 1UL << 6,         // 0x40
            CR4_PGE         = 1UL << 7,         // 0x80
            CR4_OSFXSR      = 1UL << 9,         // 0x200
            CR4_OSXMMEXCPT  = 1UL << 10,        // 0x400
            CR4_VMXE        = 1UL << 13,        // 0x2000
            CR4_SMXE        = 1UL << 14,        // 0x4000
            CR4_PCIDE       = 1UL << 17,        // 0x20000
            CR4_OSXSAVE     = 1UL << 18,        // 0x40000
            CR4_SMEP        = 1UL << 20,        // 0x100000
            CR4_SMAP        = 1UL << 21,        // 0x200000
        };

        enum
        {
            XCR0_X87       = 1UL << 0,
            XCR0_SSE       = 1UL << 1,
            XCR0_AVX       = 1UL << 2,
            XCR0_AVX512_OP = 1UL << 5,
            XCR0_AVX512_LO = 1UL << 6,
            XCR0_AVX512_HI = 1UL << 7,
        };

        enum
        {
            EFER_LME        = 1UL << 8,         // 0x100
            EFER_LMA        = 1UL << 10,        // 0x400
            EFER_SVME       = 1UL << 12,        // 0x1000
        };

        enum
        {
            EFL_CF      = 1ul << 0,             // 0x1
            EFL_PF      = 1ul << 2,             // 0x4
            EFL_AF      = 1ul << 4,             // 0x10
            EFL_ZF      = 1ul << 6,             // 0x40
            EFL_SF      = 1ul << 7,             // 0x80
            EFL_TF      = 1ul << 8,             // 0x100
            EFL_IF      = 1ul << 9,             // 0x200
            EFL_DF      = 1ul << 10,            // 0x400
            EFL_OF      = 1ul << 11,            // 0x800
            EFL_IOPL    = 3ul << 12,            // 0x3000
            EFL_NT      = 1ul << 14,            // 0x4000
            EFL_RF      = 1ul << 16,            // 0x10000
            EFL_VM      = 1ul << 17,            // 0x20000
            EFL_AC      = 1ul << 18,            // 0x40000
            EFL_VIF     = 1ul << 19,            // 0x80000
            EFL_VIP     = 1ul << 20,            // 0x100000
            EFL_ID      = 1ul << 21             // 0x200000
        };

        static mword    boot_lock           asm ("boot_lock");

        static unsigned online;
        static uint8    acpi_id[NUM_CPU];
        static uint8    apic_id[NUM_CPU];

        struct lapic_info_t {
            uint32 id, version, svr, reserved;
            uint32 lvt_timer;
            uint32 lvt_lint0;
            uint32 lvt_lint1;
            uint32 lvt_error;
            uint32 lvt_perfm;
            uint32 lvt_therm;
        };
        static lapic_info_t lapic_info[NUM_CPU];

        CPULOCAL_CONST_ACCESSOR(cpu, id);
        CPULOCAL_ACCESSOR(cpu, hazard);
        CPULOCAL_ACCESSOR(cpu, row);

        CPULOCAL_ACCESSOR(cpu, features);
        CPULOCAL_ACCESSOR(cpu, bsp);
        CPULOCAL_ACCESSOR(cpu, preemption);

        static void init();

        ALWAYS_INLINE
        static inline bool feature (Feature f)
        {
            return features()[f / 32] & 1U << f % 32;
        }

        ALWAYS_INLINE
        static inline void defeature (Feature f)
        {
            features()[f / 32] &= ~(1U << f % 32);
        }

        ALWAYS_INLINE
        static inline void preempt_disable()
        {
            assert (preemption());

            asm volatile ("cli" : : : "memory");
            preemption() = false;
        }

        ALWAYS_INLINE
        static inline void preempt_enable()
        {
            assert (!preemption());

            preemption() = true;
            asm volatile ("sti" : : : "memory");
        }

        ALWAYS_INLINE
        static inline bool preempt_status()
        {
            mword flags = 0;
            asm volatile ("pushf; pop %0" : "=r" (flags));
            return flags & 0x200;
        }

        ALWAYS_INLINE
        static unsigned find_by_apic_id (unsigned x)
        {
            for (unsigned i = 0; i < NUM_CPU; i++)
                if (apic_id[i] == x)
                    return i;

            return ~0U;
        }

        static void setup_sysenter();
};
