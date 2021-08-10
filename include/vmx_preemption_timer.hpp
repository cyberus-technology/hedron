/*
 * VMX Preemption Timer
 *
 * Copyright (C) 2021 Thomas Prescher, Cyberus Technology GmbH.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "cpulocal.hpp"
#include "msr.hpp"
#include "types.hpp"
#include "vmx.hpp"

class vmx_timer
{
    CPULOCAL_ACCESSOR(vmx, timer_shift);

public:
    // Calculate the preemption timeout value from the relative TSC ticks and
    // the timer shift.
    static uint32 calc_timeout(uint64 tsc_ticks, uint8 shift)
    {
        constexpr auto MAX_UINT32 {~static_cast<uint32>(0)};

        uint64 precision_mask {(static_cast<uint64>(1) << shift) - 1};

        // If we overflow 64bits adding our precision_mask we need the
        // maximum possible timeout.
        if (tsc_ticks + precision_mask < tsc_ticks) {
            return MAX_UINT32;
        }

        // Shift the given value to the right and rounding the
        // result up if any of the lower bits are set.
        uint64 rel_timeout {(tsc_ticks + precision_mask) >> shift};

        bool wrapped {rel_timeout != static_cast<uint32>(rel_timeout)};

        // In case the shifted value still does not fit into 32bits we use the
        // maximum possible timeout.
        return wrapped ? MAX_UINT32 : static_cast<uint32>(rel_timeout);
    }

    static void init()
    {
        /**
         * Intel SDM Vol 3, 25.5.1. VMX-Preemption Timer
         * The VMX-preemption timer counts down at rate proportional to that of the timestamp counter (TSC). Specifically,
         * the timer counts down by 1 every time bit X in the TSC changes due to a TSC increment. The value of X is in the
         * range 0–31 and can be determined by consulting the VMX capability MSR IA32_VMX_MISC (see Appendix A.6).
         */
        auto ia32_vmx_misc {Msr::read (Msr::IA32_VMX_CTRL_MISC)};
        auto timer_ratio   {ia32_vmx_misc & 0x1f};

        timer_shift() = static_cast<uint8>(timer_ratio);
    }

    static void set(uint64 relative_timeout)
    {
        /**
         * The VMCS field of the timer is only 32bit wide. This means that we
         * need to cut off the upper bits of the timeout. For a userspace VMM,
         * this means that there is the possibility that the timer fires
         * earlier than expected.
         * On the other hand, we lose precision because the provided value is
         * shifted by timer_shift. The shifted value is rounded up so the timer
         * does not fire too early due to the precision loss.
         */
        auto max_timeout {calc_timeout (relative_timeout, timer_shift())};

        Vmcs::write (Vmcs::VMX_PREEMPT_TIMER, max_timeout);
    }

    static uint64 get()
    {
        return Vmcs::read (Vmcs::VMX_PREEMPT_TIMER) << timer_shift();
    }
};
