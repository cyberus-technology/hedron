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

#pragma once

#include "config.hpp"
#include "nodestruct.hpp"
#include "optional.hpp"
#include "spinlock.hpp"

// Identifies a specific IOAPIC pin.
struct Ioapic_source {
    uint8 ioapic_id;
    uint8 ioapic_pin;
};

inline bool operator==(Ioapic_source const& lhs, Ioapic_source const& rhs)
{
    return lhs.ioapic_id == rhs.ioapic_id and lhs.ioapic_pin == rhs.ioapic_pin;
}

inline bool operator!=(Ioapic_source const& lhs, Ioapic_source const& rhs) { return not(lhs == rhs); }

class Kp;
class Sm;

// Information that is associated with a single user-programmable interrupt vector.
//
// The member fields are ordered to minimize padding. Size of this struct matters, because we keep one for
// each possible interrupt vector on each CPU.
struct Vector_info {
    Kp* kp{nullptr};
    Sm* sm{nullptr};

    uint16 kp_bit;

    // If a level-triggered interrupt routes to this vector, we need to know how to mask it.
    Optional<Ioapic_source> level_triggered_source;

    Vector_info(Kp* kp_, uint16 kp_bit_, Sm* sm_) : kp(kp_), sm(sm_), kp_bit(kp_bit_) {}
    Vector_info() = default;

    static Vector_info disabled() { return Vector_info(); }
};

// A locking wrapper around Vector_info.
class Locked_vector_info
{
    Vector_info vector_info;
    Spinlock lock;

public:
    static No_destruct<Locked_vector_info> per_vector_info[NUM_CPU][NUM_USER_VECTORS];

    // Retrieve the per-vector data in a thread-safe way.
    Vector_info get();

    // Set the per-vector data in a thread-safe way.
    //
    // It is allowed to pass nullptr for KP and SM pointers in new_vector_info. This effectively disables the
    // interrupt from being delivered to userspace.
    //
    // This function can fail, if the referenced kernel objects have already reached a reference count of zero
    // and we cannot create new references to them.
    //
    // A return value of true indicates success.
    bool set(Vector_info const& new_vector_info);

    // Mark the vector as receiving level-triggered interrupts from the IOAPIC.
    void set_level_triggered_ioapic_source(Ioapic_source source);
};
