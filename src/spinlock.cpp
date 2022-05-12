/*
 * Generic Spinlock
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "spinlock.hpp"
#include "assert.hpp"
#include "atomic.hpp"
#include "config.hpp"
#include "x86.hpp"

// We use 8-bits for the individual ticket counts in val. If we ever need more CPUs, we need to use larger
// integer types, because in the worst case each CPU can request one ticket.
static_assert(NUM_CPU < 256, "Ticket counter can overflow");

void Spinlock::lock()
{
    uint8 const our_ticket{Atomic::fetch_add<uint8, Atomic::ACQUIRE>(next_ticket, static_cast<uint8>(1))};

    while (Atomic::load<uint8, Atomic::ACQUIRE>(served_ticket) != our_ticket) {
        pause();
    }
}

void Spinlock::unlock()
{
    assert_slow(is_locked());

    // Only the lock holder modifies served_ticket, so we are free to use a non-atomic access here, because
    // there can only be other readers besides us.
    uint8 const next_served_ticket{static_cast<uint8>(served_ticket + 1)};

    Atomic::store<uint8, Atomic::RELEASE>(served_ticket, next_served_ticket);
}

bool Spinlock::is_locked() const { return Atomic::load(next_ticket) != Atomic::load(served_ticket); }
