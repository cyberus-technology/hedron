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

#include "acpi_rsdp.hpp"
#include "acpi.hpp"
#include "acpi_table.hpp"
#include "algorithm.hpp"
#include "hpt.hpp"

bool Acpi_rsdp::good_signature() const { return signature[0] == SIG("RSD ") && signature[1] == SIG("PTR "); }

bool Acpi_rsdp::good_checksum(size_t len) const { return Acpi_table::do_checksum(this, len) == 0; }

Acpi_rsdp* Acpi_rsdp::find(mword start, unsigned len)
{
    for (mword addr = start; addr < start + len; addr += 16) {
        Acpi_rsdp* rsdp = reinterpret_cast<Acpi_rsdp*>(addr);
        if (rsdp->good_signature() && rsdp->good_checksum())
            return rsdp;
    }

    return nullptr;
}

void Acpi_rsdp::parse(mword rdsp_addr)
{
    Acpi_rsdp* rsdp;

    if (rdsp_addr) {
        rsdp = reinterpret_cast<Acpi_rsdp*>(rdsp_addr);
        if (not(rsdp->good_signature() and rsdp->good_checksum()))
            return;
    } else {
        mword map = reinterpret_cast<mword>(Hpt::remap(rdsp_addr));

        if (!(rsdp = Acpi_rsdp::find(map + (*reinterpret_cast<uint16*>(map + 0x40e) << 4), 0x400)) &&
            !(rsdp = Acpi_rsdp::find(map + 0xe0000, 0x20000)))
            return;
    }

    Acpi::rsdt = rsdp->rsdt_addr;

    if (rsdp->revision > 1 && rsdp->good_checksum(rsdp->length))
        Acpi::xsdt = static_cast<Paddr>(rsdp->xsdt_addr);
}
