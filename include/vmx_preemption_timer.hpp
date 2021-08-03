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

        timer_shift() = timer_ratio;
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

    static void set(uint64 val)
    {
        auto max_timeout {static_cast<uint32>(val >> timer_shift())};

        Vmcs::write (Vmcs::VMX_PREEMPT_TIMER, max_timeout);
    }

    static uint64 get()
    {
        return Vmcs::read (Vmcs::VMX_PREEMPT_TIMER) << timer_shift();
    }
};
