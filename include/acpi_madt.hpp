/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "acpi_table.hpp"

#pragma pack(1)

/*
 * APIC Structure (5.2.11.4)
 */
class Acpi_apic
{
public:
    uint8 type;
    uint8 length;

    enum Type
    {
        LAPIC = 0,
        IOAPIC = 1,
        INTR = 2,
    };
};

/*
 * Processor Local APIC (5.2.11.5)
 */
class Acpi_lapic : public Acpi_apic
{
public:
    uint8 acpi_id;
    uint8 apic_id;
    uint32 flags;
};

/*
 * Multiple APIC Description Table
 */
class Acpi_table_madt : public Acpi_table
{
private:
    static void parse_lapic(Acpi_apic const*);

    void parse_entry(Acpi_apic::Type, void (*)(Acpi_apic const*)) const;

public:
    uint32 apic_addr;
    uint32 flags;
    Acpi_apic apic[];

    void parse() const;
};

#pragma pack()
