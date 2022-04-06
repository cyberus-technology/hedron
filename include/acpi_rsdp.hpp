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

#include "compiler.hpp"
#include "types.hpp"

/*
 * Root System Description Pointer (5.2.5)
 */
class Acpi_rsdp
{
private:
    uint32 signature[2];
    uint8 checksum;
    char oem_id[6];
    uint8 revision;
    uint32 rsdt_addr;
    uint32 length;
    uint64 xsdt_addr;
    uint8 extended_checksum;

    bool good_signature() const;
    bool good_checksum(size_t len = 20) const;

    static Acpi_rsdp* find(mword, unsigned);

public:
    static void parse(mword = 0);
};
