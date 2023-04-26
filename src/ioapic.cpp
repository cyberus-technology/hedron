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
    Pd::kern->claim_mmio_page(reg_base, paddr_ & ~PAGE_MASK);

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

    // Some BIOSes configure the I/O APIC in virtual wire mode, i.e., pin 0 is
    // set to EXTINT and left unmasked. To avoid random interrupts from being
    // delivered (e.g., when userland enables interrupts through the PIC), we
    // mask all entries and only unmask them when they are properly configured
    // by the interrupt subsystem.

    initialize_as_masked();
}

void Ioapic::set_irt_entry_uncached(size_t entry, uint64 val)
{
    assert_slow(lock.is_locked());

    write(Register(IOAPIC_IRT + 2 * entry + 1), static_cast<uint32>(val >> 32));
    write(Register(IOAPIC_IRT + 2 * entry), static_cast<uint32>(val));
}

void Ioapic::set_irt_entry_uncached_low(size_t entry, uint32 val)
{
    assert_slow(lock.is_locked());

    write(Register(IOAPIC_IRT + 2 * entry), val);
}

void Ioapic::set_irt_entry(size_t entry, uint64 val)
{
    assert_slow(lock.is_locked());

    if (shadow_redir_table[entry] >> 32 == val >> 32) {
        // Top 32-bit have not changed. This is expected to happen when we toggle the mask bit.
        set_irt_entry_uncached_low(entry, static_cast<uint32>(val));
    } else {
        set_irt_entry_uncached(entry, val);
    }

    shadow_redir_table[entry] = val;
}

uint64 Ioapic::get_irt_entry(size_t entry) const
{
    assert_slow(lock.is_locked());

    return shadow_redir_table[entry];
}

void Ioapic::set_irt_entry_compatibility(uint8 ioapic_pin, unsigned apic_id, unsigned vector, bool level,
                                         bool active_low)
{
    assert(ioapic_pin < pin_count());
    assert(vector >= 0x10 and vector <= 0xFE);

    uint64 irt_entry{vector | static_cast<uint64>(apic_id) << IRT_DESTINATION_SHIFT};

    if (active_low) {
        irt_entry |= IRT_POLARITY_ACTIVE_LOW;
    }

    if (level) {
        irt_entry |= IRT_TRIGGER_MODE_LEVEL;
    }

    Lock_guard<Spinlock> guard(lock);
    set_irt_entry(ioapic_pin, irt_entry);
}

void Ioapic::initialize_as_masked()
{
    Lock_guard<Spinlock> guard(lock);

    shadow_redir_table.resize(0);
    shadow_redir_table.resize(irt_max() + 1, IRT_MASKED);
    sync_from_shadow();
}

void Ioapic::sync_from_shadow()
{
    assert_slow(lock.is_locked());

    for (size_t i = 0; i < shadow_redir_table.size(); i++) {
        set_irt_entry_uncached(i, shadow_redir_table[i]);
    }
}

void Ioapic::set_mask(uint8 ioapic_pin, bool masked)
{
    Lock_guard<Spinlock> guard(lock);
    set_irt_entry(ioapic_pin,
                  (get_irt_entry(ioapic_pin) & ~IRT_MASKED) | (masked ? static_cast<uint64>(IRT_MASKED) : 0));
}

void Ioapic::save_all()
{
    // Nothing to be done. We will not restore the old content of the IOAPIC after resume. All devices will
    // lose their state and we expect userspace to reprogram all interrupts.
}

void Ioapic::restore_all()
{
    for (auto& opt_ioapic : ioapics_by_id) {
        if (opt_ioapic->has_value()) {
            (*opt_ioapic)->initialize_as_masked();
        }
    }
}
