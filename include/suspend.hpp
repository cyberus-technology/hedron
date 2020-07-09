/*
 * ACPI Sleep State Support
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "acpi_facs.hpp"
#include "types.hpp"

class Suspend
{
    private:

        // Set to true while suspend is ongoing.
        static inline bool in_progress {false};

        // A pristine copy of the FACS.
        //
        // Userspace might not expect the FACS to change, but during
        // suspend/resume we clobber the content of the FACS. So we keep a copy
        // around to be able to restore its content.
        static inline Acpi_table_facs saved_facs;

        // This function needs to be called on all CPUs to prepare them to
        // sleep.
        static void prepare_cpu_for_suspend();

    public:

        // Enter an ACPI Sleep State
        //
        // This function implements the bulk of the machine_ctrl suspend
        // operation. It will park all application processors, save any internal
        // state that will be lost while sleeping, flush caches and finally
        // program SLP_TYPx fields and sets the SLP_EN bit to enter the sleep
        // state. The wake vector in the FACS will be temporarily overwritten
        // and restored after the system has resumed.
        //
        // Calling this function concurrently will result in all invocations but
        // one failing.
        //
        // On a successful suspend this function will not return.
        static void suspend(uint8 slp_typa, uint8 slp_typb);

        // Clean up any state that was modified during suspend.
        static void resume_bsp() asm ("resume_bsp");
};
