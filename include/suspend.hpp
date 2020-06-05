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

#include "types.hpp"

class Suspend
{
    private:

        // Set to true while suspend is ongoing.
        static inline bool in_progress {false};

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
};
