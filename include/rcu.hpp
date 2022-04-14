/*
 * Read-Copy Update (RCU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "cpulocal.hpp"
#include "rcu_list.hpp"

/// The central Read-Copy-Update implementation.
///
/// See doc/implementation.md for a longer description of how RCU works in
/// Hedron.
class Rcu
{
private:
    static mword count;
    static mword state;

    CPULOCAL_ACCESSOR(rcu, l_batch);
    CPULOCAL_ACCESSOR(rcu, c_batch);

    CPULOCAL_ACCESSOR(rcu, next);
    CPULOCAL_ACCESSOR(rcu, curr);
    CPULOCAL_ACCESSOR(rcu, done);

    enum State
    {
        RCU_CMP = 1UL << 0,
        RCU_PND = 1UL << 1,
    };

    static inline mword batch() { return state >> 2; }

    static inline bool complete(mword b)
    {
        return static_cast<signed long>((state & ~RCU_PND) - (b << 2)) > 0;
    }

    static void start_batch(State);
    static void invoke_batch();

public:
    /// Declare the passed object ready for reclamation.
    ///
    /// This will immediately call its pre_func callback. Once the
    /// hypervisor has gone through quiescent states on all CPUs, the free
    /// callback of the object is called.
    static inline bool call(Rcu_elem* e)
    {
        if (e->pre_func)
            e->pre_func(e);

        return next().enqueue(e);
    }

    static void quiet();
    static void update();
};
