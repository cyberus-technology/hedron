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
#include "algorithm.hpp"
#include "assert.hpp"
#include "cpu.hpp"
#include "hpt.hpp"
#include "lapic.hpp"
#include "msr.hpp"
#include "tss.hpp"

alignas(PAGE_SIZE) Per_cpu Cpulocal::cpu[NUM_CPU];
alignas(PAGE_SIZE) Alt_stack Cpulocal::altstack[NUM_CPU];

Per_cpu& Cpulocal::get_remote(unsigned cpu_id)
{
    assert(cpu_id < NUM_CPU);
    return Cpulocal::cpu[cpu_id];
}

// Return the stack pointer to the alternate stack.
mword Cpulocal::alt_stack_pointer(unsigned cpu_id)
{
    assert(cpu_id < array_size(altstack));
    return reinterpret_cast<mword>(altstack[cpu_id].stack + array_size(altstack[cpu_id].stack));
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
    Hpt::unmap_kernel_page(altstack[*opt_cpu_id].stack);

    mword const gs_base{reinterpret_cast<mword>(&local.self)};
    Msr::write(Msr::IA32_GS_BASE, gs_base);
    Msr::write(Msr::IA32_KERNEL_GS_BASE, 0);

    return gs_base;
}

void Cpulocal::prevent_accidental_access()
{
    // Because our CPU-local memory is addressed via positive offsets relative to GS, we choose the last
    // canonical address for GS, which will make any (non-byte) access to GS form a non-canonical address and
    // cause a fault.
    //
    // We can't program a non-canonical address, because the CPU would give us a #GP while writing the MSR.
    Msr::write(Msr::IA32_GS_BASE, CANON_BOUND - 1);
}

bool Cpulocal::is_initialized()
{
    uint64 const gs_base{Msr::read(Msr::IA32_GS_BASE)};

    return (gs_base >= reinterpret_cast<mword>(&cpu[0])) and
           (gs_base < reinterpret_cast<mword>(&cpu[array_size(cpu)]));
}

void Cpulocal::restore_for_nmi()
{
    if (auto const opt_cpu_id{Cpu::find_by_apic_id(Lapic::early_id())}; opt_cpu_id.has_value()) {
        wrgsbase(reinterpret_cast<mword>(&Cpulocal::cpu[*opt_cpu_id].self));
    } else {
        panic("Failed to find CPU-local memory");
    }
}

bool Cpulocal::has_valid_stack()
{
    char* rsp;
    asm volatile("mov %%rsp, %0" : "=rm"(rsp));

    auto in_stack = [rsp](char(&stack)[STACK_SIZE]) -> bool {
        return rsp > &stack[0] and rsp <= &stack[STACK_SIZE];
    };

    // If CPU-local memory is not initialized, we can not check whether the stack pointer is in the correct
    // stack, but at least we see whether it is in any stack.
    if (Cpulocal::is_initialized()) {
        return in_stack(Cpulocal::get().stack);
    } else {
        for (auto& cpulocal : Cpulocal::cpu) {
            if (in_stack(cpulocal.stack)) {
                return true;
            }
        }

        return false;
    }
}
