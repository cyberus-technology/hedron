/*
 * Memory Space
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "counter.hpp"
#include "hazards.hpp"
#include "hip.hpp"
#include "lapic.hpp"
#include "lock_guard.hpp"
#include "mtrr.hpp"
#include "pd.hpp"
#include "stdio.hpp"
#include "svm.hpp"
#include "vectors.hpp"

unsigned Space_mem::did_ctr;

void Space_mem::init (unsigned cpu)
{
    cpus.set (cpu);
}

Tlb_cleanup Space_mem::update (Mdb *mdb, mword r)
{
    assert (this == mdb->space && this != &Pd::kern);

    Tlb_cleanup cleanup { r != 0 };

    Lock_guard <Spinlock> guard (mdb->node_lock);

    Paddr p = mdb->node_phys << PAGE_BITS;
    mword b = mdb->node_base << PAGE_BITS;
    mword o = mdb->node_order;
    mword a = mdb->node_attr & ~r;
    mword s = mdb->node_sub;

    if (s & 1) {
        dpt.update (cleanup, {b, p, Dpt_new::hw_attr (a), static_cast<Dpt_new::ord_t>(o + PAGE_BITS)});
    }

    if (s & 2) {
        if (Vmcb::has_npt()) {
            npt.update (cleanup, {b, p, Hpt_new::hw_attr (a), static_cast<Hpt_new::ord_t>(o + PAGE_BITS)});
        } else {
            ept.update(cleanup, {b, p, Ept_new::hw_attr (a, mdb->node_type), static_cast<Ept_new::ord_t>(o + PAGE_BITS) });
        }
        if (r)
            gtlb.merge (cpus);
    }

    if (mdb->node_base + (1UL << o) > USER_ADDR >> PAGE_BITS)
        return Tlb_cleanup::tlb_flush (false);

    hpt.update(cleanup, {b, p, Hpt_new::hw_attr (a), static_cast<Hpt_new::ord_t>(o + PAGE_BITS) });

    if (cleanup.need_tlb_flush()) {
        htlb.merge (cpus);
    }

    return cleanup;
}

void Space_mem::shootdown()
{
    for (unsigned cpu = 0; cpu < NUM_CPU; cpu++) {

        if (!Hip::cpu_online (cpu))
            continue;

        Pd *pd = Pd::remote (cpu);

        if (!pd->htlb.chk (cpu) && !pd->gtlb.chk (cpu))
            continue;

        if (Cpu::id() == cpu) {
            Cpu::hazard() |= HZD_SCHED;
            continue;
        }

        unsigned ctr = Counter::remote (cpu, 1);

        Lapic::send_ipi (cpu, VEC_IPI_RKE);

        if (!Cpu::preemption())
            asm volatile ("sti" : : : "memory");

        while (Counter::remote (cpu, 1) == ctr)
            pause();

        if (!Cpu::preemption())
            asm volatile ("cli" : : : "memory");
    }
}

void Space_mem::insert_root (uint64 s, uint64 e, mword a)
{
    for (uint64 p = s; p < e; s = p) {

        unsigned t = Mtrr::memtype (s, p);

        for (uint64 n; p < e; p = n)
            if (Mtrr::memtype (p, n) != t)
                break;

        if (s > ~0UL)
            break;

        if ((p = min (p, e)) > ~0UL)
            p = static_cast<uint64>(~0UL) + 1;

        addreg (static_cast<mword>(s >> PAGE_BITS), static_cast<mword>(p - s) >> PAGE_BITS, a, t);
    }
}

bool Space_mem::insert_utcb (mword b, mword phys)
{
    if (!b)
        return true;

    Mdb *mdb = new Mdb (this, phys, b >> PAGE_BITS, 0, 0x3);

    if (tree_insert (mdb))
        return true;

    delete mdb;

    return false;
}

bool Space_mem::remove_utcb (mword b)
{
    if (!b)
        return false;

    Mdb *mdb = tree_lookup(b >> PAGE_BITS, false);
    if (!mdb)
        return false;

    mdb->demote_node(0x3);

    if (mdb->remove_node() && tree_remove(mdb)) {
        Rcu::call (mdb);
        return true;
    }

    return false;
}
