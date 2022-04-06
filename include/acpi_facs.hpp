/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "acpi_gas.hpp"
#include "acpi_table.hpp"

#pragma pack(1)

/*
 * Firmware ACPI Control Structure (5.2.10)
 */
class Acpi_table_facs : public Acpi_header
{
public:
    uint32 hardware_signature;       // 8
    uint32 firmware_waking_vector;   // 12
    uint32 global_lock;              // 16
    uint32 flags;                    // 20
    uint64 x_firmware_waking_vector; // 24
    uint8 version;                   // 32
    uint8 reserved[3];               // 33
    uint32 ospm_flags;               // 36
    uint8 reserved_2[24];            // 40

    enum Flags
    {
        S4BIOS_F = 1u << 0,
        WAKE_64BIT_SUPPORTED_F = 1u << 1,
    };

    enum Ospm_flags
    {
        WAKE_64BIT_F = 1u << 0,
    };
};

static_assert(sizeof(Acpi_table_facs) == 64);

#pragma pack()
