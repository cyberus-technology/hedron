/*
 * Timeout
 *
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "timeout.hpp"
#include "assert.hpp"
#include "lapic.hpp"
#include "x86.hpp"

void Timeout::enqueue(uint64 t)
{
    assert(prev == nullptr);
    assert(next == nullptr);

    time = t;

    Timeout* p = nullptr;

    for (Timeout* n = list(); n; p = n, n = n->next)
        if (n->time >= time)
            break;

    prev = p;

    if (!p) {
        next = list();
        list() = this;
        Lapic::set_timer(time);
    } else {
        next = p->next;
        p->next = this;
    }

    if (next)
        next->prev = this;
}

uint64 Timeout::dequeue()
{
    if (active()) {

        if (next)
            next->prev = prev;

        if (prev)
            prev->next = next;

        else if ((list() = next))
            Lapic::set_timer(list()->time);
    }

    prev = next = nullptr;

    return time;
}

void Timeout::check()
{
    Timeout* prev_list = list();

    while (list() && list()->time <= rdtsc()) {
        Timeout* t = list();
        t->dequeue();
        t->trigger();
    }

    if (list() && (list() == prev_list)) {
        /*
         * No timeout was dequeued, which can happen if the TSC stops in CPU
         * sleep states (non-invariant TSC). In that case, we program the
         * LAPIC again for the next timeout.
         */
        Lapic::set_timer(list()->time);
    }
}
