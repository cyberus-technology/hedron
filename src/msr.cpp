/*
 * Model-Specific Registers (MSR)
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

#include "msr.hpp"

uint64 Msr::read(Register msr)
{
    uint32 h, l;
    asm volatile("rdmsr" : "=a"(l), "=d"(h) : "c"(msr));
    return (static_cast<uint64>(h) << 32) | l;
}

bool Msr::read_safe(Register msr, uint64& val)
{
    uint32 h{0}, l{0};
    bool skipped;

    asm volatile(FIXUP_CALL(rdmsr) : FIXUP_SKIPPED(skipped), "+a"(l), "+d"(h) : "c"(msr));

    val = (static_cast<uint64>(h) << 32) | l;
    return not skipped;
}

void Msr::write(Register msr, uint64 val)
{
    asm volatile("wrmsr" : : "a"(static_cast<mword>(val)), "d"(static_cast<mword>(val >> 32)), "c"(msr));
}

bool Msr::write_safe(Register msr, uint64 val)
{
    bool skipped;

    asm volatile(FIXUP_CALL(wrmsr)
                 : FIXUP_SKIPPED(skipped)
                 : "a"(static_cast<mword>(val)), "d"(static_cast<mword>(val >> 32)), "c"(msr));

    return not skipped;
}

// Below is the code for userspace MSR access. It is mostly used for platform
// thermal and power management, but also for platform discovery and other
// purposes. Due to the vast amount of MSRs it's impractical to devise safe and
// generic kernel abstractions and instead we rely on trusted userspace
// components (PDs with passthrough permissions) to do the right thing and give
// them direct access to MSRs.
//
// That being said, we do not blindly allow access to all MSRs, but make
// exceptions for MSRs where we know that:
//
// - the hypervisor wholly owns them and needs them for correct functionality,
//
// - they leak private information about the hypervisor, such as its address
//   space layout,
//
// - have a correct way to get their information from userspace already.
//
// The lists below are not complete and will never be. They are expected to
// change over time (as is our rationale which MSRs to exclude).
//
// Most importantly this filtering is not a security boundary and no untrusted
// component should have access to this API.

static bool is_allowed_to_read(Msr::Register msr)
{
    // Allowing read access to an MSR might currently imply granting write
    // access as well. Check is_allowed_to_write when modifying this list.
    switch (msr) {
    case Msr::AMD_SVM_HSAVE_PA:
    case Msr::IA32_APIC_BASE:
    case Msr::IA32_DS_AREA:
    case Msr::IA32_EFER:
    case Msr::IA32_GS_BASE:
    case Msr::IA32_KERNEL_GS_BASE:
    case Msr::IA32_SYSENTER_CS:
    case Msr::IA32_SYSENTER_EIP:
    case Msr::IA32_SYSENTER_ESP:
    case Msr::IA32_TSC:
    case Msr::IA32_TSC_ADJUST:
    case Msr::IA32_TSC_AUX:
    case Msr::IA32_TSC_DEADLINE:

    case Msr::IA32_EXT_XAPIC... Msr::IA32_EXT_XAPIC_END:
    case Msr::IA32_VMX_BASIC... Msr::IA32_VMX_VMFUNC:
        return false;

    // Userspace discovers whether SGX is available via the relevant
    // feature bits in the IA32_FEATURE_CONTROL MSR.
    case Msr::IA32_FEATURE_CONTROL:
    // Allow runtime modification of SGX Launch Control.
    case Msr::IA32_SGXLEPUBKEYHASH0:
    case Msr::IA32_SGXLEPUBKEYHASH1:
    case Msr::IA32_SGXLEPUBKEYHASH2:
    case Msr::IA32_SGXLEPUBKEYHASH3:
    default:
        return true;
    };
}

static bool is_allowed_to_write(Msr::Register msr)
{
    switch (msr) {
    // Feature control is locked in try_enable_vmx, so writes are not permitted.
    case Msr::IA32_FEATURE_CONTROL:
    case Msr::IA32_MTRR_PHYS_BASE... Msr::IA32_MTRR_FIX4K_F8000:
    case Msr::IA32_CR_PAT:
    case Msr::IA32_MTRR_DEF_TYPE:
        // The MTRR MSRs are mostly in a convenient block in MSR space. We allow
        // reading them to pass on the configuration to guests (if so desired),
        // but writing them would mess up our paging.
        return false;

    // Allow runtime modification of SGX Launch Control.
    case Msr::IA32_SGXLEPUBKEYHASH0... Msr::IA32_SGXLEPUBKEYHASH3:
        return true;

    default:
        // If we don't know anything better and we can't read a MSR, we
        // shouldn't be able to write it either.
        return is_allowed_to_read(msr);
    }
}

bool Msr::user_write(Register msr, uint64 val)
{
    if (not is_allowed_to_write(msr)) {
        return false;
    }

    return write_safe(msr, val);
}

bool Msr::user_read(Register msr, uint64& val)
{
    if (not is_allowed_to_read(msr)) {
        val = 0;
        return false;
    }

    return read_safe(msr, val);
}
