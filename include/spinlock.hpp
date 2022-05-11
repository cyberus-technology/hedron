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

#include "types.hpp"

// A spinlock implementation based on a ticket lock.
//
// The spinlock is best used via the Lock_guard class to avoid mismatched lock/unlock calls.
class Spinlock
{
private:
    // The upper byte of this value is the next ticket that will be given out. The lower byte is the ticket
    // being currently served.
    uint16 val{0};

public:
    void lock();
    void unlock();

    // Check whether the lock is currently locked.
    //
    // This method is _only_ useful for assertions.
    bool is_locked() const;
};
