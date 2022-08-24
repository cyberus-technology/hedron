/*
 * Early Bootstrap Code
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "atomic.hpp"
#include "compiler.hpp"
#include "types.hpp"

/// Initialization for the kernel after initial boot or a suspend/resume cycle.
///
/// See doc/implementation.md for a general overview of the boot flow.
class Bootstrap
{
    /// A spinlock that serializes CPU initialization.
    static inline mword boot_lock asm("boot_lock");

    static void release_next_cpu() { Atomic::store(boot_lock, static_cast<mword>(1)); }

    /// A counter to implement the CPU boot barrier.
    static inline mword barrier;

    /// Spin until all processors have reached this code.
    static void wait_for_all_cpus();

    /// Reset the boot synchronization logic for another initialization
    /// pass.
    static void rearm()
    {
        barrier = 0;
        boot_lock = 0;
    }

    /// Create the idle EC.
    static void create_idle_ec();

    /// Create the initial PD and EC for the rootask.
    static void create_roottask();

public:
    [[noreturn]] static void bootstrap() asm("bootstrap");
};
