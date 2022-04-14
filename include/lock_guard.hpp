/*
 * Generic Lock Guard
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2015 Alexander Boettcher, Genode Labs GmbH
 *
 * This file is part of the Hedron microhypervisor.
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

#include "compiler.hpp"
#include "cpu.hpp"

template <typename T> class Lock_guard
{
private:
    T& _lock;

public:
    inline Lock_guard(T& l) : _lock(l)
    {
        // Attempting to grab a lock while preemptible. This is a bug.
        assert(!Cpu::preemptible());

        _lock.lock();
    }

    inline ~Lock_guard() { _lock.unlock(); }
};
