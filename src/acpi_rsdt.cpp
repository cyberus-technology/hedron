/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "acpi_rsdt.hpp"
#include "acpi.hpp"
#include "hpt.hpp"

struct Acpi_table_rsdt::table_map const Acpi_table_rsdt::map[] = {
    {SIG("APIC"), &Acpi::madt},
    {SIG("DMAR"), &Acpi::dmar},
    {SIG("FACP"), &Acpi::fadt},
    {SIG("MCFG"), &Acpi::mcfg},
};

void Acpi_table_rsdt::parse(Paddr addr, size_t size) const
{
    if (!good_checksum(addr))
        return;

    unsigned long count = entries(size);

    Paddr table[count];
    for (unsigned i = 0; i < count; i++)
        table[i] = static_cast<Paddr>(size == sizeof(xsdt_) ? xsdt(i) : rsdt(i));

    for (unsigned i = 0; i < count; i++) {

        Acpi_table* acpi = static_cast<Acpi_table*>(Hpt::remap(table[i]));

        if (acpi->good_checksum(table[i]))
            for (unsigned j = 0; j < sizeof map / sizeof *map; j++)
                if (acpi->signature == map[j].sig)
                    *map[j].ptr = table[i];
    }
}
