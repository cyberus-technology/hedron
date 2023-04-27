/*
 * Read-Copy Update (RCU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "rcu.hpp"
#include "atomic.hpp"
#include "barrier.hpp"
#include "cpu.hpp"
#include "hazards.hpp"
#include "hip.hpp"
#include "initprio.hpp"
#include "lapic.hpp"
#include "stdio.hpp"
#include "vectors.hpp"

mword Rcu::state = RCU_CMP;
mword Rcu::count;

void Rcu::invoke_batch()
{
    for (Rcu_elem *e = done().head, *n = nullptr; n != done().head; e = n) {
        n = e->next;
        e->next = nullptr;
        (e->func)(e);
    }

    done().clear();
}

void Rcu::start_batch(State s)
{
    mword v, m = RCU_CMP | RCU_PND;

    do
        if ((v = state) >> 2 != l_batch())
            return;
    while (!(v & s) && !Atomic::cmp_swap(state, v, v | s));

    if ((v ^ ~s) & m)
        return;

    count = Cpu::online;

    barrier();

    state++;
}

void Rcu::quiet()
{
    if (Atomic::sub(count, 1UL) == 0)
        start_batch(RCU_CMP);
}

void Rcu::update()
{
    if (l_batch() != batch()) {
        l_batch() = batch();
        Atomic::set_mask(Cpu::hazard(), HZD_RCU);
    }

    if (!curr().empty() && complete(c_batch()))
        done().append(&curr());

    if (curr().empty() && !next().empty()) {
        curr().append(&next());

        c_batch() = l_batch() + 1;

        start_batch(RCU_PND);
    }

    if (!curr().empty() && !next().empty() && (next().count > 2000 || curr().count > 2000))
        for (unsigned cpu = 0; cpu < NUM_CPU; cpu++) {

            if (!Hip::cpu_online(cpu) || Cpu::id() == cpu)
                continue;

            Lapic::send_ipi(cpu, VEC_IPI_IDL);
        }

    if (!done().empty())
        invoke_batch();
}
