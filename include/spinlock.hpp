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

#pragma once

#include "assert.hpp"
#include "atomic.hpp"
#include "config.hpp"
#include "types.hpp"
#include "x86.hpp"

// A spinlock implementation based on a ticket lock.
//
// The spinlock is best used via the Lock_guard class to avoid mismatched lock/unlock calls.
class Spinlock
{
private:
    using Ticket = uint8;

    // We use 8-bits for the individual ticket counts in val. If we ever need more CPUs, we need to use larger
    // integer types, because in the worst case each CPU can request one ticket.
    static_assert(NUM_CPU < 256, "Ticket counter can overflow");

    // The next ticket that we will give out.
    Ticket next_ticket{0};

    // The ticket that may enter the critical section.
    Ticket served_ticket{0};

public:
    void lock()
    {
        Ticket const our_ticket{
            Atomic::fetch_add<Ticket, Atomic::ACQUIRE>(next_ticket, static_cast<Ticket>(1))};

        while (Atomic::load<Ticket, Atomic::ACQUIRE>(served_ticket) != our_ticket) {
            pause();
        }
    }

    void unlock()
    {
        assert_slow(is_locked());

        // Only the lock holder modifies served_ticket, so we are free to use a non-atomic access here,
        // because there can only be other readers besides us.
        Ticket const next_served_ticket{static_cast<Ticket>(served_ticket + 1)};

        Atomic::store<Ticket, Atomic::RELEASE>(served_ticket, next_served_ticket);
    }

    // Check whether the lock is currently locked.
    //
    // This method is _only_ useful for positive assertions, i.e. to check whether a spinlock is currently
    // held.
    bool is_locked() const { return Atomic::load(next_ticket) != Atomic::load(served_ticket); }
};
