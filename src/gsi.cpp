/*
 * Global System Interrupts (GSI)
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

#include "gsi.hpp"
#include "acpi.hpp"
#include "dmar.hpp"
#include "ioapic.hpp"
#include "lapic.hpp"
#include "sm.hpp"
#include "vectors.hpp"

Gsi Gsi::gsi_table[NUM_GSI];

void Gsi::setup()
{
    for (unsigned gsi = 0; gsi < NUM_GSI; gsi++) {

        Space_obj::insert_root(Gsi::gsi_table[gsi].sm = new Sm(&Pd::kern, NUM_CPU + gsi));
    }
}

void Gsi::configure_ioapic_irt(unsigned gsi, unsigned cpu, bool level, bool active_low)
{
    Ioapic* ioapic = gsi_table[gsi].ioapic;
    uint32 aid = Cpu::apic_id[cpu];

    assert(ioapic);

    if (Dmar::ire()) {
        ioapic->set_irt_entry_remappable(gsi, gsi, VEC_GSI + gsi, level, active_low);
    } else {
        ioapic->set_irt_entry_compatibility(gsi, aid, VEC_GSI + gsi, level, active_low);
    }

    Dmar::set_irt(gsi, ioapic->get_rid(), aid, VEC_GSI + gsi, level);
}

uint64 Gsi::configure_msi(unsigned gsi, unsigned cpu, unsigned rid)
{
    assert(!gsi_table[gsi].ioapic);

    uint32 aid = Cpu::apic_id[cpu];
    uint32 msi_addr = 0xfee00000 | (Dmar::ire() ? 3U << 3 : aid << 12);
    uint32 msi_data = Dmar::ire() ? gsi : (VEC_GSI + gsi);

    Dmar::set_irt(gsi, rid, aid, VEC_GSI + gsi, false /* edge */);

    return static_cast<uint64>(msi_addr) << 32 | msi_data;
}

void Gsi::unmask(unsigned gsi)
{
    Ioapic* ioapic = gsi_table[gsi].ioapic;

    // We only actively mask level-triggered interrupts (see vector below).
    if (ioapic)
        ioapic->unmask_if_level(gsi);
}

void Gsi::vector(unsigned vector)
{
    unsigned gsi = vector - VEC_GSI;

    // Level-triggered interrupts would fire immediately again after we signal EOI to the Local APIC. To avoid
    // this, mask them at the IOAPIC.
    if (gsi_table[gsi].ioapic) {
        gsi_table[gsi].ioapic->mask_if_level(gsi);
    }

    Lapic::eoi();

    gsi_table[gsi].sm->submit();
}
