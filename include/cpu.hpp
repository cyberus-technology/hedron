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

#include "assert.hpp"
#include "config.hpp"
#include "cpuinfo.hpp"
#include "cpulocal.hpp"
#include "optional.hpp"
#include "types.hpp"

class Cpu
{
private:
    static char const* const vendor_string[];

    static inline Cpu_info check_features();

    static inline void setup_thermal();

public:
    enum Feature
    {
        FEAT_MCE = 7,
        FEAT_SEP = 11,
        FEAT_MCA = 14,
        FEAT_ACPI = 22,
        FEAT_HTT = 28,
        FEAT_MONITOR = 35,
        FEAT_VMX = 37,
        FEAT_PCID = 49,
        FEAT_TSC_DEADLINE = 56,
        FEAT_XSAVE = 58,
        FEAT_FSGSBASE = 96,
        FEAT_SMEP = 103,
        FEAT_SMAP = 116,
        FEAT_1GB_PAGES = 154,
        FEAT_CMP_LEGACY = 161,
        FEAT_XSAVEOPT = 192,

        FEAT_IBRS_IBPB = 7 * 32 + 26,
        FEAT_STIBP = 7 * 32 + 27,
        FEAT_L1D_FLUSH = 7 * 32 + 28,
        FEAT_ARCH_CAP = 7 * 32 + 29,
        FEAT_SSBD = 7 * 32 + 31,

        // Synthetic features that do not correspond to CPUID bits.
        FEAT_IA32_SPEC_CTRL = 8 * 32,
    };

    enum
    {
        EXC_DB = 1,
        EXC_NMI = 2,
        EXC_NM = 7,
        EXC_DF = 8,
        EXC_TS = 10,
        EXC_GP = 13,
        EXC_PF = 14,
        EXC_AC = 17,
        EXC_MC = 18,
    };

    enum
    {
        CR0_PE = 1ul << 0,  // 0x1
        CR0_MP = 1ul << 1,  // 0x2
        CR0_EM = 1ul << 2,  // 0x4
        CR0_TS = 1ul << 3,  // 0x8
        CR0_ET = 1ul << 4,  // 0x10
        CR0_NE = 1ul << 5,  // 0x20
        CR0_WP = 1ul << 16, // 0x10000
        CR0_AM = 1ul << 18, // 0x40000
        CR0_NW = 1ul << 29, // 0x20000000
        CR0_CD = 1ul << 30, // 0x40000000
        CR0_PG = 1ul << 31  // 0x80000000
    };

    enum
    {
        CR4_DE = 1UL << 3,          // 0x8
        CR4_PSE = 1UL << 4,         // 0x10
        CR4_PAE = 1UL << 5,         // 0x20
        CR4_MCE = 1UL << 6,         // 0x40
        CR4_PGE = 1UL << 7,         // 0x80
        CR4_OSFXSR = 1UL << 9,      // 0x200
        CR4_OSXMMEXCPT = 1UL << 10, // 0x400
        CR4_VMXE = 1UL << 13,       // 0x2000
        CR4_SMXE = 1UL << 14,       // 0x4000
        CR4_FSGSBASE = 1UL << 16,   // 0x10000
        CR4_PCIDE = 1UL << 17,      // 0x20000
        CR4_OSXSAVE = 1UL << 18,    // 0x40000
        CR4_SMEP = 1UL << 20,       // 0x100000
        CR4_SMAP = 1UL << 21,       // 0x200000
    };

    enum
    {
        XCR0_X87 = 1UL << 0,
        XCR0_SSE = 1UL << 1,
        XCR0_AVX = 1UL << 2,
        XCR0_AVX512_OP = 1UL << 5,
        XCR0_AVX512_LO = 1UL << 6,
        XCR0_AVX512_HI = 1UL << 7,
    };

    enum
    {
        EFER_LME = 1UL << 8,  // 0x100
        EFER_LMA = 1UL << 10, // 0x400
    };

    enum
    {
        EFL_CF = 1ul << 0,    // 0x1
        EFL_MBS = 1ul << 1,   // 0x2 "must-be-set"
        EFL_PF = 1ul << 2,    // 0x4
        EFL_AF = 1ul << 4,    // 0x10
        EFL_ZF = 1ul << 6,    // 0x40
        EFL_SF = 1ul << 7,    // 0x80
        EFL_TF = 1ul << 8,    // 0x100
        EFL_IF = 1ul << 9,    // 0x200
        EFL_DF = 1ul << 10,   // 0x400
        EFL_OF = 1ul << 11,   // 0x800
        EFL_IOPL = 3ul << 12, // 0x3000
        EFL_NT = 1ul << 14,   // 0x4000
        EFL_RF = 1ul << 16,   // 0x10000
        EFL_VM = 1ul << 17,   // 0x20000
        EFL_AC = 1ul << 18,   // 0x40000
        EFL_VIF = 1ul << 19,  // 0x80000
        EFL_VIP = 1ul << 20,  // 0x100000
        EFL_ID = 1ul << 21    // 0x200000
    };

    static inline unsigned online;
    static inline uint8 acpi_id[NUM_CPU];
    static inline uint8 apic_id[NUM_CPU];

    static inline uint32 bsp_lapic_svr;
    static inline uint32 bsp_lapic_lint0;

    // The TSC value that all CPUs start with after boot or resume.
    static inline uint64 initial_tsc = 0;

    CPULOCAL_CONST_ACCESSOR(cpu, id);
    CPULOCAL_REMOTE_ACCESSOR(cpu, hazard);

    // Access the hazards on a remote core. Has to be accessed using atomic ops!
    static unsigned& hazard(unsigned cpu) { return remote_ref_hazard(cpu); }

    CPULOCAL_REMOTE_ACCESSOR(cpu, might_loose_nmis);

    CPULOCAL_ACCESSOR(cpu, features);
    CPULOCAL_ACCESSOR(cpu, bsp);
    CPULOCAL_ACCESSOR(cpu, maxphyaddr_ord);

    static Cpu_info init();

    // Partially update CPU features. This is useful after a microcode
    // change that may have added features.
    static void update_features();

    static inline bool feature(Feature f) { return features()[f / 32] & 1U << f % 32; }

    static inline void defeature(Feature f) { features()[f / 32] &= ~(1U << f % 32); }

    static inline void set_feature(Feature f, bool on)
    {
        uint32& value = features()[f / 32];
        uint32 const mask = 1U << f % 32;

        value = (value & ~mask) | (on ? mask : 0);
    }

    /// Check whether the CPU is _actually_ preemptible by returning
    /// RFLAGS.IF. See also the description of preempt_enabled above.
    static inline bool preemptible()
    {
        mword flags = 0;
        asm volatile("pushf; pop %0" : "=r"(flags));
        return flags & 0x200;
    }

    // Return the CPU number from a Local APIC ID, if we have one.
    //
    // This function can fail, if there are more CPUs in the system than Hedron was compiled for (NUM_CPU).
    static Optional<unsigned> find_by_apic_id(unsigned apic_id);

    static void setup_msrs();
};
