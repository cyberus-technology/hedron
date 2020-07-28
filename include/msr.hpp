/*
 * Model-Specific Registers (MSR)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "arch.hpp"
#include "compiler.hpp"
#include "types.hpp"

class Msr
{
    public:

        enum Register
        {
            // Architectural MSRs (Intel)
            IA32_TSC                = 0x10,
            IA32_PLATFORM_ID        = 0x17,
            IA32_APIC_BASE          = 0x1b,
            IA32_FEATURE_CONTROL    = 0x3a,
            IA32_TSC_ADJUST         = 0x3b,
            IA32_SPEC_CTRL          = 0x48,
            IA32_PRED_CMD           = 0x49,
            IA32_BIOS_UPDT_TRIG     = 0x79,
            IA32_BIOS_SIGN_ID       = 0x8b,
            IA32_SMM_MONITOR_CTL    = 0x9b,
            IA32_MTRR_CAP           = 0xfe,
            IA32_ARCH_CAP           = 0x10a,
            IA32_FLUSH_CMD          = 0x10b,
            IA32_SYSENTER_CS        = 0x174,
            IA32_SYSENTER_ESP       = 0x175,
            IA32_SYSENTER_EIP       = 0x176,
            IA32_MCG_CAP            = 0x179,
            IA32_MCG_STATUS         = 0x17a,
            IA32_MCG_CTL            = 0x17b,
            IA32_THERM_INTERRUPT    = 0x19b,
            IA32_THERM_STATUS       = 0x19c,
            IA32_MISC_ENABLE        = 0x1a0,
            IA32_DEBUG_CTL          = 0x1d9,
            IA32_MTRR_PHYS_BASE     = 0x200,
            IA32_MTRR_PHYS_MASK     = 0x201,
            IA32_MTRR_FIX64K_BASE   = 0x250,
            IA32_MTRR_FIX16K_BASE   = 0x258,
            IA32_MTRR_FIX4K_BASE    = 0x268,
            IA32_MTRR_FIX4K_F8000   = 0x26f,
            IA32_CR_PAT             = 0x277,
            IA32_MTRR_DEF_TYPE      = 0x2ff,

            IA32_MCI_CTL            = 0x400,
            IA32_MCI_STATUS         = 0x401,

            IA32_VMX_BASIC          = 0x480,
            IA32_VMX_CTRL_PIN       = 0x481,
            IA32_VMX_CTRL_CPU0      = 0x482,
            IA32_VMX_CTRL_EXIT      = 0x483,
            IA32_VMX_CTRL_ENTRY     = 0x484,
            IA32_VMX_CTRL_MISC      = 0x485,
            IA32_VMX_CR0_FIXED0     = 0x486,
            IA32_VMX_CR0_FIXED1     = 0x487,
            IA32_VMX_CR4_FIXED0     = 0x488,
            IA32_VMX_CR4_FIXED1     = 0x489,
            IA32_VMX_VMCS_ENUM      = 0x48a,
            IA32_VMX_CTRL_CPU1      = 0x48b,
            IA32_VMX_EPT_VPID       = 0x48c,

            IA32_VMX_TRUE_PIN       = 0x48d,
            IA32_VMX_TRUE_CPU0      = 0x48e,
            IA32_VMX_TRUE_EXIT      = 0x48f,
            IA32_VMX_TRUE_ENTRY     = 0x490,
            IA32_VMX_VMFUNC         = 0x491,

            IA32_DS_AREA            = 0x600,
            IA32_TSC_DEADLINE       = 0x6e0,
            IA32_EXT_XAPIC          = 0x800,
            IA32_EXT_XAPIC_END      = 0x8ff,
            IA32_EFER               = 0xc0000080,
            IA32_STAR               = 0xc0000081,
            IA32_LSTAR              = 0xc0000082,
            IA32_FMASK              = 0xc0000084,
            IA32_FS_BASE            = 0xc0000100,
            IA32_GS_BASE            = 0xc0000101,
            IA32_KERNEL_GS_BASE     = 0xc0000102,
            IA32_TSC_AUX            = 0xc0000103,

            // Non-architectural MSRs (Intel)
            MSR_PLATFORM_INFO       = 0xce,

            // AMD-specific MSRs
            AMD_IPMR                = 0xc0010055,
            AMD_SVM_HSAVE_PA        = 0xc0010117,
        };

        enum Feature_Control
        {
            FEATURE_LOCKED          = 1ul << 0,
            FEATURE_VMX_I_SMX       = 1ul << 1,
            FEATURE_VMX_O_SMX       = 1ul << 2
        };

        // Access MSRs without safety net.
        //
        // Care must be taken to ensure that the MSR actually exists and the
        // value that is written is sane, because there is no error handling and
        // the kernel will panic with a #GP in case something invalid is
        // accessed.
        static uint64 read (Register msr);
        static void write (Register msr, uint64 val);

        // Same as read/write above, but any access that causes #GP is
        // ignored. That means writes are dropped and reads return 0.
        static uint64 read_safe (Register msr);
        static void write_safe (Register msr, uint64 val);

        // Access MSRs from userspace for passthrough PDs.
        static bool user_write (Register msr, uint64  val);
        static bool user_read  (Register msr, uint64 &val);
};
