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

#include "buddy.hpp"
#include "ioapic.hpp"
#include "pd.hpp"
#include "stdio.hpp"

Ioapic *Ioapic::list;

void *Ioapic::operator new (size_t size)
{
    assert (size == sizeof (Ioapic));
    static_assert (sizeof (Ioapic) <= PAGE_SIZE);

    return Buddy::allocator.alloc (0, Buddy::NOFILL);
}

Ioapic::Ioapic (Paddr p, unsigned i, unsigned g) : Forward_list<Ioapic> (list), paddr(uint32(p)), reg_base ((hwdev_addr -= PAGE_SIZE) | (p & PAGE_MASK)), gsi_base (g), id (i), rid (0)
{
    Pd::kern->claim_mmio_page (reg_base, p & ~PAGE_MASK);

    trace (TRACE_APIC, "APIC:%#lx ID:%#x VER:%#x IRT:%#x PRQ:%u GSI:%u",
           p, i, version(), irt_max(), prq(), gsi_base);

    // Some BIOSes configure the I/O APIC in virtual wire mode, i.e., pin 0 is
    // set to EXTINT and left unmasked. To avoid random interrupts from being
    // delivered (e.g., when userland enables interrupts through the PIC), we
    // mask all entries and only unmask them when they are properly configured
    // by the GSI subsystem.
    for (unsigned pin {0}; pin <= irt_max(); pin++) {
        set_irt(gsi_base + pin, 1U << 16);
    }
}

void Ioapic::set_irt_entry (size_t entry, uint64 val)
{
    write (Register (IOAPIC_IRT + 2 * entry + 1), static_cast<uint32> (val >> 32));
    write (Register (IOAPIC_IRT + 2 * entry), static_cast<uint32> (val));
}

uint64 Ioapic::get_irt_entry (size_t entry)
{
    return (static_cast<uint64> (read (Register (IOAPIC_IRT + 2 * entry + 1))) << 32)
        | read (Register (IOAPIC_IRT + 2 * entry));
}

void Ioapic::save()
{
    size_t entries {irt_max()};

    suspend_redir_table.reset();

    for (size_t i = 0; i < entries; i++) {
        suspend_redir_table.push_back (get_irt_entry(i));
    }
}

void Ioapic::restore()
{
    for (size_t i = 0; i < suspend_redir_table.size(); i++) {
        set_irt_entry(i, suspend_redir_table[i]);
    }
}

void Ioapic::save_all()
{
    for_each (Forward_list_range {list}, mem_fn_closure(&Ioapic::save)());
}

void Ioapic::restore_all()
{
    for_each (Forward_list_range {list}, mem_fn_closure(&Ioapic::restore)());
}
