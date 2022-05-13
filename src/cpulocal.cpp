/*
 * CPU local data structures
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "cpulocal.hpp"
#include "assert.hpp"
#include "cpu.hpp"
#include "hpt.hpp"
#include "lapic.hpp"
#include "msr.hpp"
#include "tss.hpp"

alignas(PAGE_SIZE) Per_cpu Cpulocal::cpu[NUM_CPU];

Per_cpu& Cpulocal::get_remote(unsigned cpu_id)
{
    assert(cpu_id < NUM_CPU);
    return Cpulocal::cpu[cpu_id];
}

mword Cpulocal::setup_cpulocal()
{
    auto const opt_cpu_id{Cpu::find_by_apic_id(Lapic::early_id())};

    if (not opt_cpu_id.has_value()) {
        // Abort CPU bootstrap.
        return 0;
    }

    unsigned const cpu_id{*opt_cpu_id};
    Per_cpu& local{cpu[cpu_id]};

    local.cpu_id = cpu_id;

    // Establish stack guard page by unmapping the last (lowest) page of the stack.
    static_assert(STACK_SIZE > PAGE_SIZE, "Stack is too small to place a stack guard");
    Hpt::unmap_kernel_page(local.stack);

    mword const gs_base{reinterpret_cast<mword>(&local.self)};
    Msr::write(Msr::IA32_GS_BASE, gs_base);
    Msr::write(Msr::IA32_KERNEL_GS_BASE, 0);

    return gs_base;
}
