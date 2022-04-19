/*
 * Scheduling Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "compiler.hpp"
#include "cpulocal.hpp"

class Ec;

class Sc : public Typed_kobject<Kobject::Type::SC>, public Refcount
{
    friend class Queue<Sc>;

public:
    Refptr<Ec> const ec;
    unsigned const cpu;
    unsigned const prio;
    uint64 const budget;
    uint64 time;

private:
    uint64 left;
    Sc *prev, *next;
    uint64 tsc;

    static Slab_cache cache;

    CPULOCAL_REMOTE_ACCESSOR(sc, rq);
    CPULOCAL_ACCESSOR(sc, list);
    CPULOCAL_ACCESSOR(sc, prio_top);

    void ready_enqueue(uint64, bool);

    void ready_dequeue(uint64);

    static void free(Rcu_elem* a)
    {
        Sc* s = static_cast<Sc*>(a);

        if (s->del_ref()) {
            assert(Sc::current() != s);
            delete s;
        }
    }

public:
    // Capability permission bitmask.
    enum
    {
        PERM_SC_CTRL = 1U << 0,

        PERM_ALL = PERM_SC_CTRL,
    };

    CPULOCAL_ACCESSOR(sc, current);
    CPULOCAL_ACCESSOR(sc, ctr_link);
    CPULOCAL_ACCESSOR(sc, ctr_loop);

    static unsigned const default_prio = 1;
    static unsigned const default_quantum = 10000;

    Sc(Pd*, mword, Ec*);
    Sc(Pd*, mword, Ec*, unsigned, unsigned, unsigned);

    // Access the runqueue on a remote core.
    //
    // The returned pointer is valid forever as it points to statically
    // allocated memory.
    static Rq* remote(unsigned cpu) { return &remote_ref_rq(cpu); }

    void remote_enqueue(bool = true);

    static void rrq_handler();
    static void rke_handler();

    NORETURN
    static void schedule(bool = false);

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
