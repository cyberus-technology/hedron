/*
 * Bootstrap Code
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "bootstrap.hpp"
#include "compiler.hpp"
#include "ec.hpp"
#include "hip.hpp"
#include "lapic.hpp"
#include "msr.hpp"
#include "stdio.hpp"

void Bootstrap::bootstrap()
{
    // If we already have the idle EC, we've been here before, and we go
    // through here as part of resume from ACPI sleep states.
    bool const is_initial_boot = not Ec::idle_ec();

    if (Cpu_info cpu_info = Cpu::init(); is_initial_boot) {
        Hip::add_cpu(cpu_info);
    }

    // Let the next CPU initialize itself. From now one, we can only touch
    // CPU-local data.
    release_next_cpu();

    if (is_initial_boot) {
        create_idle_ec();
    }

    wait_for_all_cpus();

    // We need to set the TSC immediately after the barrier finishes to be sure
    // that all CPUs execute this at a roughly identical time. This does not
    // achieve perfect synchronization between TSCs, but should be good enough.
    //
    // By using TSC_ADJUST, we could achieve perfect TSC synchronization, but
    // experiments in the past have uncovered CPU bugs: See the following forum
    // post for details:
    //
    // https://community.intel.com/t5/Processors/Missing-TSC-deadline-interrupt-after-suspend-resume-and-using/td-p/287889
    Msr::write(Msr::IA32_TSC, Cpu::initial_tsc);

    if (Cpu::bsp()) {
        // All CPUs are online. Time to restore the low memory that we've
        // clobbered for booting APs.
        Lapic::restore_low_memory();

        if (is_initial_boot) {
            Hip::finalize();
            create_roottask();
        }
    }

    Sc::schedule();
}

void Bootstrap::wait_for_all_cpus()
{
    // Announce that we entered the barrier.
    Atomic::add(barrier, 1UL);

    // Wait for everyone else to arrive.
    while (Atomic::load(barrier) != Cpu::online) {
        relax();
    }
}

void Bootstrap::create_idle_ec()
{
    Ec::idle_ec() = new Ec(Pd::current() = &Pd::kern, Cpu::id());
    Ec::current() = Ec::idle_ec();

    Ec::current()->add_ref();
    Pd::current()->add_ref();
    Space_obj::insert_root(Sc::current() = new Sc(&Pd::kern, Cpu::id(), Ec::current()));
    Sc::current()->add_ref();
}

void Bootstrap::create_roottask()
{
    ALIGNED(32) static No_destruct<Pd> root(&root, NUM_EXC, 0x1, Pd::IS_PRIVILEGED | Pd::IS_PASSTHROUGH);

    Ec* root_ec =
        new Ec(&root, NUM_EXC + 1, &root, Ec::root_invoke, Cpu::id(), 0, USER_ADDR - 2 * PAGE_SIZE, 0, 0);
    Sc* root_sc = new Sc(&root, NUM_EXC + 2, root_ec, Cpu::id(), Sc::default_prio, Sc::default_quantum);
    root_sc->remote_enqueue();
}
