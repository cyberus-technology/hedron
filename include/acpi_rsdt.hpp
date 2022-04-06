/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "acpi_table.hpp"

#pragma pack(1)

/*
 * Root System Description Table (5.2.7 and 5.2.8)
 */
class Acpi_table_rsdt : public Acpi_table
{
private:
    static struct table_map {
        uint32 const sig;
        Paddr* const ptr;
    } const map[];

    unsigned long entries(size_t size) const { return (length - sizeof(Acpi_table)) / size; }

    union {
        uint32 rsdt_;
        uint64 xsdt_;
    };

public:
    uint32 rsdt(unsigned i) const { return *(&rsdt_ + i); }
    uint64 xsdt(unsigned i) const { return *(&xsdt_ + i); }

    void parse(Paddr, size_t) const;
};

#pragma pack()
