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
    static void init()
    {
        auto ia32_vmx_misc {Msr::read (Msr::IA32_VMX_CTRL_MISC)};
        auto timer_ratio   {ia32_vmx_misc & 0x1f};

        timer_shift() = static_cast<uint8>(timer_ratio);
    }

    static bool active()
    {
        uint64 pin = Vmcs::read (Vmcs::PIN_CONTROLS);
        return pin & Vmcs::PIN_PREEMPT_TIMER;
    }

    static void activate()
    {
        if (active()) {
            return;
        }

        // Enable preemption timer
        uint64 pin = Vmcs::read (Vmcs::PIN_CONTROLS);
        pin |= Vmcs::PIN_PREEMPT_TIMER;
        Vmcs::write (Vmcs::PIN_CONTROLS, pin);

        // Enable saving preemption timer value
        uint64 exi = Vmcs::read (Vmcs::EXI_CONTROLS);
        exi |= Vmcs::EXI_SAVE_PREEMPT_TIMER;
        Vmcs::write (Vmcs::EXI_CONTROLS, exi);
    }

    static void deactivate()
    {
        if (not active()) {
            return;
        }

        // Disable preemption timer
        uint64 pin = Vmcs::read (Vmcs::PIN_CONTROLS);
        pin &= ~Vmcs::PIN_PREEMPT_TIMER;
        Vmcs::write (Vmcs::PIN_CONTROLS, pin);

        // Disable saving preemption timer value
        uint64 exi = Vmcs::read (Vmcs::EXI_CONTROLS);
        exi &= ~Vmcs::EXI_SAVE_PREEMPT_TIMER;
        Vmcs::write (Vmcs::EXI_CONTROLS, exi);
    }

    static void set(uint64 relative_timeout)
    {
        // Calculate the preemption timeout value from TSC value and the timer
        // shift. Shift the given value to the right and rounding the result
        // up if any of the lower bits are set. The function takes care that we
        // do not overflow in case we round up.
        const auto calc_timeout = [](uint64 tsc_value, uint8 shift) -> uint32 {
            constexpr auto MAX_UINT32 {mask (sizeof(uint32) * 8)};
            const     auto SHIFT_MASK {mask (shift)};

            auto round_up {(tsc_value & SHIFT_MASK) == 0 ? 0u : 1u};
            auto shifted  {tsc_value >> shift};

            return static_cast<uint32>(shifted >= MAX_UINT32 ? shifted : shifted + round_up);
        };

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
