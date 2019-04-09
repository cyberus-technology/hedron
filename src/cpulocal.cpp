/*
 * CPU local data structures
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "cpu.hpp"
#include "cpulocal.hpp"
#include "lapic.hpp"
#include "msr.hpp"
#include "tss.hpp"

alignas(PAGE_SIZE) Per_cpu Cpulocal::cpu[NUM_CPU];

Per_cpu &Cpulocal::get_remote(unsigned cpu_id)
{
    assert (cpu_id < NUM_CPU);
    return Cpulocal::cpu[cpu_id];
}

INIT
mword Cpulocal::setup_cpulocal()
{
    unsigned cpu_id {Cpu::find_by_apic_id (Lapic::early_id())};
    Per_cpu &local {cpu[cpu_id]};

    mword gs_base {reinterpret_cast<mword>(&local.self)};
    Msr::write<mword> (Msr::IA32_GS_BASE, gs_base);
    Msr::write<mword> (Msr::IA32_KERNEL_GS_BASE, 0);

    return gs_base;
}
