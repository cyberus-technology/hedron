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
#include "lapic.hpp"
#include "lock_guard.hpp"
#include "rcu.hpp"
#include "sm.hpp"
#include "stdio.hpp"
#include "vectors.hpp"

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

void Locked_vector_info::clear_level_triggered_ioapic_source()
{
    Lock_guard g{lock};

    if (auto const& old_src{vector_info.level_triggered_source}; old_src.has_value()) {
        Ioapic::by_id(old_src->ioapic_id)->set_mask(old_src->ioapic_pin, true);
    }

    vector_info.level_triggered_source = Optional<Ioapic_source>();
}

bool Locked_vector_info::handle_user_interrupt()
{
    // This spinlock can only contended, if the vector is being reprogrammed. This is rare.
    Lock_guard g{lock};
    bool handled{false};

    if (auto& src{vector_info.level_triggered_source}; src.has_value()) {
        Ioapic::by_id(src->ioapic_id)->set_mask(src->ioapic_pin, true);
    }

    if (EXPECT_TRUE(vector_info.sm)) {
        // We have the semaphore, we also must have the KP, otherwise we have a bug in the way we set these
        // pointers.
        assert_slow(vector_info.kp);

        auto* const kp_bits{static_cast<Bitmap<mword, PAGE_SIZE * 8>*>(vector_info.kp->data_page())};

        // We trigger a semaphore up only when we initially set a bit. If the bit was already set, userspace
        // already got a semaphore up earlier.
        //
        // We could further optimize this by only sending the semaphore up if none of the bits in the modified
        // mword in the bitfield has been zero before, but this would require userspace to not use the bits in
        // the kpage for anything beyond interrupt notification.
        if (not kp_bits->atomic_fetch_set(vector_info.kp_bit)) {
            vector_info.sm->up();
        }

        handled = true;
    }

    Lapic::eoi();

    return handled;
}

void Locked_vector_info::handle_user_interrupt(unsigned host_vector)
{
    assert_slow(host_vector >= VEC_USER and host_vector < (VEC_USER + NUM_USER_VECTORS));
    unsigned const user_vector{host_vector - VEC_USER};

    if (EXPECT_TRUE(per_vector_info[Cpu::id()][user_vector]->handle_user_interrupt())) {
        return;
    }

    // If we get here, we received an interrupt, but userspace does not care about it.
    trace(TRACE_ERROR, "Spurious IRQ on vector %u", user_vector);
}
