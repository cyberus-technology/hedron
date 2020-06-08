/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "acpi_table.hpp"
#include "algorithm.hpp"
#include "stdio.hpp"

uint8 Acpi_table::do_checksum(const void *table, size_t len)
{
    return static_cast<uint8>(accumulate(static_cast<uint8 const *>(table),
                                         static_cast<uint8 const *>(table) + len,
                                         0));
}

bool Acpi_table::good_checksum (Paddr addr) const
{
    bool valid {do_checksum() == 0};

    trace (TRACE_ACPI, "%.4s:%#010llx REV:%2d TBL:%8.8s OEM:%6.6s LEN:%5u (%s)",
           reinterpret_cast<char const *>(&signature),
           static_cast<uint64>(addr),
           revision,
           oem_table_id,
           oem_id,
           length,
           valid ? "ok" : "bad");

    return valid;
}
