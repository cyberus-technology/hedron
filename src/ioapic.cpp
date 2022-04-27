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
#include "dmar.hpp"
#include "lock_guard.hpp"
#include "pd.hpp"
#include "stdio.hpp"

Ioapic* Ioapic::list;

void* Ioapic::operator new(size_t size)
{
    assert(size == sizeof(Ioapic));
    static_assert(sizeof(Ioapic) <= PAGE_SIZE);

    return Buddy::allocator.alloc(0, Buddy::NOFILL);
}

Ioapic::Ioapic(Paddr paddr_, unsigned id_, unsigned gsi_base_)
    : Forward_list<Ioapic>(list), paddr(uint32(paddr_)),
      reg_base((hwdev_addr -= PAGE_SIZE) | (paddr_ & PAGE_MASK)), gsi_base(gsi_base_), id(id_), rid(0)
{
    Pd::kern->claim_mmio_page(reg_base, paddr_ & ~PAGE_MASK);

    trace(TRACE_APIC, "APIC:%#x ID:%#x VER:%#x IRT:%#x PRQ:%u GSI:%u", paddr, id, version(), irt_max(), prq(),
          gsi_base);

    // Some BIOSes configure the I/O APIC in virtual wire mode, i.e., pin 0 is
    // set to EXTINT and left unmasked. To avoid random interrupts from being
    // delivered (e.g., when userland enables interrupts through the PIC), we
    // mask all entries and only unmask them when they are properly configured
    // by the GSI subsystem.

    shadow_redir_table.resize(irt_max() + 1, IRT_MASKED);

    // At this point, we are not sure what the actual values in the IRT are. So we ignore the cache for the
    // inital write.
    restore();
}

void Ioapic::set_irt_entry_uncached(size_t entry, uint64 val)
{
    write(Register(IOAPIC_IRT + 2 * entry + 1), static_cast<uint32>(val >> 32));
    write(Register(IOAPIC_IRT + 2 * entry), static_cast<uint32>(val));
}

void Ioapic::set_irt_entry_uncached_low(size_t entry, uint32 val)
{
    write(Register(IOAPIC_IRT + 2 * entry), val);
}

void Ioapic::set_irt_entry(size_t entry, uint64 val)
{
    if (shadow_redir_table[entry] >> 32 == val >> 32) {
        // Top 32-bit have not changed. This is expected to happen when we toggle the mask bit.
        set_irt_entry_uncached_low(entry, static_cast<uint32>(val));
    } else {
        set_irt_entry_uncached(entry, val);
    }

    shadow_redir_table[entry] = val;
}

uint64 Ioapic::get_irt_entry(size_t entry) const { return shadow_redir_table[entry]; }

void Ioapic::set_irt_entry_compatibility(unsigned gsi, unsigned apic_id, unsigned vector, bool level,
                                         bool active_low)
{
    unsigned const pin{gsi - gsi_base};
    assert_slow(pin <= irt_max());
    assert(vector >= 0x10 and vector <= 0xFE);
    assert(not Dmar::ire());

    uint64 irt_entry{vector | static_cast<uint64>(apic_id) << IRT_DESTINATION_SHIFT};

    if (active_low) {
        irt_entry |= IRT_POLARITY_ACTIVE_LOW;
    }

    if (level) {
        irt_entry |= IRT_TRIGGER_MODE_LEVEL;
    }

    Lock_guard<Spinlock> guard(lock);
    set_irt_entry(pin, irt_entry);
}

void Ioapic::set_irt_entry_remappable(unsigned gsi, unsigned iommu_irt_index, unsigned vector, bool level,
                                      bool active_low)
{
    unsigned const pin{gsi - gsi_base};
    assert_slow(pin <= irt_max());
    assert(iommu_irt_index <= 0xffff);
    assert(Dmar::ire());

    // See Section 5.1.5.1 I/OxAPIC Programming in the Intel VT-d specification for information about how to
    // program the IOAPIC IRTs.
    //
    // To handle level-triggered interrupts correctly, we must not set the Subhandle Valid (SHV) bit in the
    // resulting MSI message. This allows us to configure trigger mode, polarity and the destination vector in
    // the IOAPIC IRT. Programming these are necessary to make EOI broadcasts from the LAPIC work.

    uint64 irt_entry{IRT_FORMAT_REMAPPABLE | vector |
                     (static_cast<uint64>(iommu_irt_index & 0x7fff) << IRT_REMAPPABLE_HANDLE_0_15_SHIFT) |
                     (static_cast<uint64>(iommu_irt_index >> 16) << IRT_REMAPPABLE_HANDLE_16_SHIFT)};

    if (active_low) {
        irt_entry |= IRT_POLARITY_ACTIVE_LOW;
    }

    if (level) {
        irt_entry |= IRT_TRIGGER_MODE_LEVEL;
    }

    Lock_guard<Spinlock> guard(lock);
    set_irt_entry(pin, irt_entry);
}

void Ioapic::mask_if_level(unsigned gsi)
{
    unsigned const pin{gsi - gsi_base};
    assert_slow(pin <= irt_max());

    Lock_guard<Spinlock> guard(lock);

    uint64 const old_entry{shadow_redir_table[pin]};

    if ((old_entry & (IRT_TRIGGER_MODE_LEVEL | IRT_MASKED)) != IRT_TRIGGER_MODE_LEVEL) {
        // Not level or already masked.
        return;
    }

    set_irt_entry(pin, old_entry | IRT_MASKED);
}

void Ioapic::unmask_if_level(unsigned gsi)
{
    unsigned const pin{gsi - gsi_base};
    assert_slow(pin <= irt_max());

    Lock_guard<Spinlock> guard(lock);

    uint64 const old_entry{shadow_redir_table[pin]};

    if ((old_entry & (IRT_TRIGGER_MODE_LEVEL | IRT_MASKED)) != (IRT_TRIGGER_MODE_LEVEL | IRT_MASKED)) {
        // Not level or not masked.
        return;
    }

    set_irt_entry(pin, old_entry & ~IRT_MASKED);
}

void Ioapic::restore()
{
    Lock_guard<Spinlock> guard(lock);

    for (size_t i = 0; i < shadow_redir_table.size(); i++) {
        set_irt_entry_uncached(i, shadow_redir_table[i]);
    }
}

void Ioapic::save_all()
{
    // Nothing to be done. shadow_redir_table is a write-through cache of the IOAPIC state and has everything
    // we need.
}

void Ioapic::restore_all() { for_each(Forward_list_range{list}, mem_fn_closure(&Ioapic::restore)()); }
