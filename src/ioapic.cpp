/*
 * I/O Advanced Programmable Interrupt Controller (IOAPIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "ioapic.hpp"
#include "buddy.hpp"
#include "lock_guard.hpp"
#include "pd.hpp"
#include "stdio.hpp"

No_destruct<Optional<Ioapic>> Ioapic::ioapics_by_id[NUM_IOAPIC];

Ioapic::Ioapic(Paddr paddr_, unsigned id_, unsigned gsi_base_)
    : paddr(uint32(paddr_)), reg_base((hwdev_addr -= PAGE_SIZE) | (paddr_ & PAGE_MASK)), gsi_base(gsi_base_),
      id(id_), rid(0)
{
    Pd::kern->claim_mmio_page(reg_base, paddr_ & ~PAGE_MASK, false);

    uint32 const id_reg{read_id_reg()};

    trace(TRACE_APIC, "IOAPIC:%#x ID:%#x VER:%#x IRT:%#x GSI:%u", paddr, id_reg, version(), irt_max(),
          gsi_base);

    uint32 const hw_id{(id_reg >> ID_SHIFT) & ID_MASK};
    if (hw_id != id) {
        trace(TRACE_ERROR, "BIOS bug? Got ID %#x from MADT, but %#x from IOAPIC! Fixing.", id, hw_id);

        // We believe the BIOS. The Linux kernel is not even treating mismatching IOAPIC IDs as much of a
        // special case and just silently reassigns the ID to the IOAPIC.
        write(IOAPIC_ID, id << ID_SHIFT);
    }
}
