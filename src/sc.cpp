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

#include "counter.hpp"
#include "ec.hpp"
#include "lapic.hpp"
#include "stdio.hpp"
#include "time.hpp"
#include "timeout_budget.hpp"
#include "vectors.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Sc::cache(sizeof(Sc), 32);

Sc::Sc(Pd* own, mword sel, Ec* e)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, Sc::PERM_ALL, free), ec(e),
      cpu(static_cast<unsigned>(sel)), prio(0), budget(us_as_ticks_in_freq(Lapic::freq_tsc, ONE_SEC_IN_US)),
      left(0), prev(nullptr), next(nullptr)
{
    trace(TRACE_SYSCALL, "SC:%p created (PD:%p Kernel)", this, own);
}

Sc::Sc(Pd* own, mword sel, Ec* e, unsigned c, unsigned p, unsigned q)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, Sc::PERM_ALL, free), ec(e), cpu(c), prio(p),
      budget(us_as_ticks_in_freq(Lapic::freq_tsc, q)), left(0), prev(nullptr), next(nullptr)
{
    trace(TRACE_SYSCALL, "SC:%p created (EC:%p CPU:%#x P:%#x Q:%#x)", this, e, c, p, q);
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
        if (left)
            list()[prio] = this;
    }

    trace(TRACE_SCHEDULE, "ENQ:%p (%llu) PRIO:%#x TOP:%#x %s", this, left, prio, prio_top(),
          prio > current()->prio ? "reschedule" : "");

    if (prio > current()->prio || (this != current() && prio == current()->prio && left))
        Cpu::hazard() |= HZD_SCHED;

    if (!left)
        left = budget;

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

    trace(TRACE_SCHEDULE, "DEQ:%p (%llu) PRIO:%#x TOP:%#x", this, left, prio, prio_top());

    ec->add_tsc_offset(tsc - t);

    tsc = t;
}

void Sc::schedule(bool suspend)
{
    assert(current());
    assert(suspend || !current()->prev);

    uint64 t = rdtsc();
    uint64 d = Timeout_budget::budget()->dequeue();

    current()->time += t - current()->tsc;
    current()->left = d > t ? d - t : 0;

    Cpu::hazard() &= ~HZD_SCHED;

    if (EXPECT_TRUE(!suspend))
        current()->ready_enqueue(t, false);
    else if (current()->del_rcu())
        Rcu::call(current());

    Sc* sc = list()[prio_top()];
    assert(sc);

    Timeout_budget::budget()->enqueue(t + sc->left);

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
            Lapic::send_ipi(cpu, VEC_IPI_RRQ);
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

void Sc::rke_handler()
{
    // By increasing the TLB shootdown handler, we tell Space_mem::shootdown()
    // that this CPU is going to perform any necessary TLB invalidations before
    // returning to userspace.
    //
    // We increase the counter as early as possible to avoid making the
    // shootdown loop wait for longer than needed.

    Atomic::add(Counter::tlb_shootdown(), static_cast<uint32>(1));

    // In case of host TLB invalidations, we need to enforce that we go through
    // the scheduler, because there we call Pd::make_current, which performs the
    // invalidation. Otherwise, we would re-enter userspace directly from the
    // interrupt handler.
    //
    // For guest TLB invalidations, there is no need to go through the
    // scheduler, because ret_user_vmresume will take care of
    // guest TLB invalidations unconditionally.

    if (Pd::current()->Space_mem::stale_host_tlb.chk(Cpu::id()))
        Cpu::hazard() |= HZD_SCHED;
}
