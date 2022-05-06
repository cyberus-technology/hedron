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
#include "atomic.hpp"
#include "config.hpp"

// We use 8-bits for the individual ticket counts in val. If we ever need more CPUs, we need to use larger
// integer types, because in the worst case each CPU can request one ticket.
static_assert(NUM_CPU < 256, "Ticket counter can overflow");

void Spinlock::lock()
{
    uint16 tmp = 0x100;

    // Enqueue ourselves into the ticket lock and wait until our ticket is served.
    asm volatile("     lock; xadd %0, %1;  "
                 "1:   cmpb %h0, %b0;      "
                 "     je 2f;              "
                 "     pause;              "
                 "     movb %1, %b0;       "
                 "     jmp 1b;             "
                 "2:                       "
                 : "+Q"(tmp), "+m"(val)
                 :
                 : "memory");
}

void Spinlock::unlock()
{
    // Update the "now-serving" part of the ticket lock. Only the lock holder modifies this value, so no
    // atomic operation is required.
    //
    // For non-x86 architectures, we would need a `release` fence. But for x86 this is a no-op and the
    // memory clobber does just fine.
    asm volatile("incb %0" : "+m"(val) : : "memory");
}
