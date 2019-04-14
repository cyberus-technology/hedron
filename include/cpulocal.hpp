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

#pragma once

#include "memory.hpp"
#include "config.hpp"
#include "compiler.hpp"
#include "types.hpp"

class Ec;
class Pd;

// This struct defines the layout of CPU-local memory. It's designed to make it
// convenient to use %gs:0 to restore the stack pointer and to get a normal
// pointer to CPU-local variables. The first members in the Per_cpu struct are
// frequently accessed and are deliberately placed on a single cache line.
//
//                 +---------------------+
//                 | ... other vars ...  |
//                 +---------------------+
//     %gs:8       | sys entry stack ptr |
//                 +---------------------+
//     %gs:0       | self pointer        |
//                 +---------------------+
//                 |                     |
//                 | Kernel Stack        |
//                 | (grows downwards)   |
//                 |                     |
//     %gs:-0x1000 +---------------------+

struct alignas(PAGE_SIZE) Per_cpu {
    char stack[PAGE_SIZE];

    // This member is pointed to by %gs:0 and points to itself, i.e. the stack
    // pointer value for an empty stack.
    void *self {&self};

    // The system call entry point dumps userspace state here.
    void *sys_entry_stack {nullptr};

    // The APIC ID of the current CPU.
    unsigned cpu_id;

    // Any special conditions that need to be checked on kernel entry/exit
    // paths. See hazards.hpp.
    unsigned cpu_hazard;

    // The current execution context.
    Ec *ec_current;

    // The current protection domain.
    Pd *pd_current;
};

static_assert(OFFSETOF(Per_cpu, self)            == PAGE_SIZE,
              "This offset is used in assembly, grep Per_cpu::self to find them");
static_assert(OFFSETOF(Per_cpu, sys_entry_stack) == PAGE_SIZE + 8,
              "This offset is used in assembly, grep Per_cpu::sys_entry_stack to find them");

class Cpulocal
{
        static Per_cpu cpu[NUM_CPU];

    public:

        static Per_cpu &get()
        {
            char *r;
            asm ("mov %%gs:0, %0" : "=r" (r));
            return *reinterpret_cast<Per_cpu *>(r - OFFSETOF(Per_cpu, self));
        }

        static Per_cpu &get_remote(unsigned cpu_id);

        // Set up CPU local memory for the current CPU. Returns the stack pointer.
        static mword setup_cpulocal() asm ("setup_cpulocal");

        template <typename T, size_t OFFSET>
        static T get_field()
        {
            T res;
            asm ("mov %%gs:%1, %0" : "=r" (res) : "m" (*reinterpret_cast<T *>(OFFSET - OFFSETOF(Per_cpu, self))));
            return res;
        }

        template <typename T, size_t OFFSET>
        static void set_field(T value)
        {
            asm ("mov %1, %%gs:%0" : "=m" (*reinterpret_cast<T *>(OFFSET - OFFSETOF(Per_cpu, self))) : "r" (value));
        }

        static void set_sys_entry_stack (void *es)
        {
            set_field<decltype (Per_cpu::sys_entry_stack), OFFSETOF(Per_cpu, sys_entry_stack)>(es);
        }
};

// Helper macros for the accessor macros below.
#define CPULOCAL_FIELD_NAME(prefix, field) prefix ## _ ## field
#define CPULOCAL_TYPE(prefix, field) decltype(Per_cpu::CPULOCAL_FIELD_NAME(prefix, field))

// Generate a static method to read or write a CPU-local variable.
//
// The macro takes prefix and field parameters and expects that a prefix_field
// variable exists in struct Per_cpu. It generates a method called field.
#define CPULOCAL_ACCESSOR(prefix, field) static auto field() -> CPULOCAL_TYPE(prefix, field) & \
    { return Cpulocal::get().CPULOCAL_FIELD_NAME(prefix, field); }

// Same as CPULOCAL_ACCESSOR, but generates a (slightly more efficient)
// read-only accessor.
#define CPULOCAL_CONST_ACCESSOR(prefix, field) static auto field() -> CPULOCAL_TYPE(prefix, field) \
    { return Cpulocal::get_field<CPULOCAL_TYPE(prefix, field), OFFSETOF(Per_cpu, CPULOCAL_FIELD_NAME(prefix, field))>(); }
