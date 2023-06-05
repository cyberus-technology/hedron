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

#include "sc.hpp"
#include "counter.hpp"
#include "ec.hpp"
#include "lapic.hpp"
#include "stdio.hpp"
#include "time.hpp"
#include "vectors.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Sc::cache(sizeof(Sc), 32);

Sc::Sc(Pd* own, mword sel, Ec* e)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, Sc::PERM_ALL, free), ec(e),
      cpu(static_cast<unsigned>(sel)), prio(0), prev(nullptr), next(nullptr)
{
    trace(TRACE_SYSCALL, "SC:%p created (PD:%p Kernel)", this, own);
}

Sc::Sc(Pd* own, mword sel, Ec* e, unsigned c, unsigned p)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, Sc::PERM_ALL, free), ec(e), cpu(c), prio(p),
      prev(nullptr), next(nullptr)
{
    trace(TRACE_SYSCALL, "SC:%p created (EC:%p CPU:%#x P:%#x)", this, e, c, p);
}

void Sc::ready_enqueue(uint64 t, bool inc_ref)
{
    assert(prio < NUM_PRIORITIES);
    assert(cpu == Cpu::id());

    if (inc_ref) {
        bool ok = add_ref();
        assert(ok);
        if (!ok)
            return;
    }

    if (prio > prio_top())
        prio_top() = prio;

    if (!list()[prio])
        list()[prio] = prev = next = this;
    else {
        next = list()[prio];
        prev = list()[prio]->prev;
        next->prev = prev->next = this;
    }

    trace(TRACE_SCHEDULE, "ENQ:%p PRIO:%#x TOP:%#x %s", this, prio, prio_top(),
          prio > current()->prio ? "reschedule" : "");

    if (prio > current()->prio) {
        Atomic::set_mask(Cpu::hazard(), HZD_SCHED);
    }

    tsc = t;
}

void Sc::ready_dequeue(uint64 t)
{
    assert(prio < NUM_PRIORITIES);
    assert(cpu == Cpu::id());
    assert(prev && next);

    if (list()[prio] == this)
        list()[prio] = next == this ? nullptr : next;

    next->prev = prev;
    prev->next = next;
    prev = next = nullptr;

    while (!list()[prio_top()] && prio_top())
        prio_top()--;

    trace(TRACE_SCHEDULE, "DEQ:%p PRIO:%#x TOP:%#x", this, prio, prio_top());

    tsc = t;
}

void Sc::schedule(bool suspend)
{
    assert(current());
    assert(suspend || !current()->prev);

    const uint64 t = rdtsc();
    current()->time += t - current()->tsc;

    if (EXPECT_TRUE(!suspend))
        current()->ready_enqueue(t, false);
    else if (current()->del_rcu())
        Rcu::call(current());

    Sc* sc = list()[prio_top()];
    assert(sc);

    ctr_loop() = 0;

    current() = sc;
    sc->ready_dequeue(t);
    sc->ec->activate();
}

void Sc::remote_enqueue(bool inc_ref)
{
    if (Cpu::id() == cpu)
        ready_enqueue(rdtsc(), inc_ref);

    else {
        if (inc_ref) {
            bool ok = add_ref();
            assert(ok);
            if (!ok)
                return;
        }

        Rq* r = remote(cpu);

        Lock_guard<Spinlock> guard(r->lock);

        if (r->queue) {
            next = r->queue;
            prev = r->queue->prev;
            next->prev = prev->next = this;
        } else {
            r->queue = prev = next = this;
            Atomic::set_mask(Cpu::hazard(cpu), HZD_RRQ);
            Lapic::send_nmi(cpu);
        }
    }
}

void Sc::rrq_handler()
{
    uint64 t = rdtsc();

    Lock_guard<Spinlock> guard(rq().lock);

    for (Sc* ptr = rq().queue; ptr;) {

        ptr->next->prev = ptr->prev;
        ptr->prev->next = ptr->next;

        Sc* sc = ptr;

        ptr = ptr->next == ptr ? nullptr : ptr->next;

        sc->ready_enqueue(t, false);
    }

    rq().queue = nullptr;
}
