/*
 * Per-interrupt vector configuration
 *
 * Copyright (C) 2022 Julian Stecklina, Cyberus Technology GmbH.
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

#include "vector_info.hpp"
#include "ioapic.hpp"
#include "kp.hpp"
#include "lock_guard.hpp"
#include "rcu.hpp"
#include "sm.hpp"

No_destruct<Locked_vector_info> Locked_vector_info::per_vector_info[NUM_CPU][NUM_USER_VECTORS];

Vector_info Locked_vector_info::get()
{
    Lock_guard g{lock};
    return vector_info;
}

bool Locked_vector_info::set(Vector_info const& new_vector_info)
{
    // We create and destroy kernel object references here that last beyond one kernel invocation. We need to
    // adjust their reference counts.
    if (new_vector_info.kp and new_vector_info.sm) {

        if (not new_vector_info.kp->add_ref()) {
            return false;
        }

        if (not new_vector_info.sm->add_ref()) {
            if (new_vector_info.kp->del_ref()) {
                Rcu::call(new_vector_info.kp);
            }

            return false;
        }
    } else {
        // The calling code makes sure that we either have none or both capability pointers.
        assert(!new_vector_info.kp);
        assert(!new_vector_info.sm);
    }

    Vector_info old_vector_info;

    {
        // We avoid calling into RCU with the spinlock held. The spinlock is grabbed in the interrupt path and
        // holding it for a long time causes interrupt delay.
        Lock_guard g{lock};

        old_vector_info = vector_info;
        vector_info = new_vector_info;

        // There might have been a level triggered interrupt routed to this CPU/vector pair before. If so, we
        // need to shut it up to avoid an interrupt storm.
        //
        // We do this with the spinlock held to make it easier to reason about what happens in case of
        // concurrent calls to set().
        auto const& old_src{old_vector_info.level_triggered_source};
        if (old_src.has_value() and old_src != new_vector_info.level_triggered_source) {
            Ioapic::by_id(old_src->ioapic_id)->set_mask(old_src->ioapic_pin, true);
        }
    }

    if (old_vector_info.kp and old_vector_info.kp->del_ref()) {
        Rcu::call(old_vector_info.kp);
    }

    if (old_vector_info.sm and old_vector_info.sm->del_ref()) {
        Rcu::call(old_vector_info.sm);
    }

    return true;
}

void Locked_vector_info::set_level_triggered_ioapic_source(Ioapic_source source)
{
    Lock_guard g{lock};

    if (auto const& old_src{vector_info.level_triggered_source}; old_src.has_value() and *old_src != source) {
        Ioapic::by_id(old_src->ioapic_id)->set_mask(old_src->ioapic_pin, true);
    }

    vector_info.level_triggered_source = source;
}
