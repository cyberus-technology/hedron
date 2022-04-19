/*
 * Generic Spinlock
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

#pragma once

#include "compiler.hpp"
#include "types.hpp"

// A spinlock implementation based on a ticket lock.
class Spinlock
{
private:
    // The upper byte of this value is the next ticket that will be given out. The lower byte is the ticket
    // being currently served.
    uint16 val;

public:
    inline Spinlock() : val(0) {}

    NOINLINE
    void lock()
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

    inline void unlock()
    {
        // Update the "now-serving" part of the ticket lock. Only the lock holder modifies this value, so no
        // atomic operation is required.
        //
        // For non-x86 architectures, we would need a `release` fence. But for x86 this is a no-op and the
        // memory clobber does just fine.
        asm volatile("incb %0" : "+m"(val) : : "memory");
    }
};
