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

#pragma once

#include "compiler.hpp"
#include "config.hpp"
#include "gdt.hpp"
#include "memory.hpp"
#include "rcu_list.hpp"
#include "rq.hpp"
#include "types.hpp"
#include "vmx_types.hpp"

class Ec;
class Pd;
class Sc;
class Timeout;
class Vmcs;

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
    void* const self{const_cast<void**>(&self)};

    // The system call entry point dumps userspace state here.
    void* sys_entry_stack{nullptr};

    // The APIC ID of the current CPU.
    unsigned cpu_id;

    // Any special conditions that need to be checked on kernel entry/exit
    // paths. See hazards.hpp.
    unsigned cpu_hazard;

    // The current execution context.
    Ec* ec_current;

    // The current protection domain.
    Pd* pd_current;

    // The current scheduling context.
    Sc* sc_current;

    // The current virtual machine control structure.
    Vmcs* vmcs_current;

    // Ec-related variables;
    Ec* ec_idle_ec;

    // The list of pending timeouts.
    Timeout* timeout_list;
    Timeout* timeout_budget;

    // Scheduling-related variables
    Rq sc_rq;
    Sc* sc_list[NUM_PRIORITIES];
    unsigned sc_prio_top;
    unsigned sc_ctr_link;
    unsigned sc_ctr_loop;

    // VMX-related variables
    unsigned vmcs_vpid_ctr;
    vmx_basic vmcs_basic;
    vmx_ept_vpid vmcs_ept_vpid;
    vmx_ctrl_pin vmcs_ctrl_pin;
    vmx_ctrl_cpu vmcs_ctrl_cpu[2];
    vmx_ctrl_exi vmcs_ctrl_exi;
    vmx_ctrl_ent vmcs_ctrl_ent;

    mword vmcs_fix_cr0_set;
    mword vmcs_fix_cr0_clr;
    mword vmcs_fix_cr0_mon;

    mword vmcs_fix_cr4_set;
    mword vmcs_fix_cr4_clr;
    mword vmcs_fix_cr4_mon;

    uint8 vmx_timer_shift;

    // SVM-related variables
    Paddr vmcb_root;
    unsigned vmcb_asid_ctr;
    uint32 vmcb_svm_version;
    uint32 vmcb_svm_feature;

    // Statistics
    uint32 counter_tlb_shootdown;

    // CPU-related variables (that are not performance critical)
    uint32 cpu_features[9];
    bool cpu_bsp;

    // Machine-check variables
    unsigned mca_banks;

    // Read-copy update
    mword rcu_l_batch;
    mword rcu_c_batch;
    Rcu_list rcu_next;
    Rcu_list rcu_curr;
    Rcu_list rcu_done;

    // Global descriptor table
    alignas(8) Gdt::Gdt_array gdt;
};

static_assert(OFFSETOF(Per_cpu, self) == PAGE_SIZE,
              "This offset is used in assembly, grep Per_cpu::self to find them");
static_assert(OFFSETOF(Per_cpu, sys_entry_stack) == PAGE_SIZE + 8,
              "This offset is used in assembly, grep Per_cpu::sys_entry_stack to find them");

class Cpulocal
{
    static Per_cpu cpu[NUM_CPU];

public:
    static Per_cpu& get()
    {
        char* r;
        asm("mov %%gs:0, %0" : "=r"(r));
        return *reinterpret_cast<Per_cpu*>(r - OFFSETOF(Per_cpu, self));
    }

    static Per_cpu& get_remote(unsigned cpu_id);

    // Set up CPU local memory for the current CPU. Returns the stack pointer.
    static mword setup_cpulocal() asm("setup_cpulocal");

    template <typename T, size_t OFFSET> static T get_field()
    {
        T res;
        asm("mov %%gs:%1, %0" : "=r"(res) : "m"(*reinterpret_cast<T*>(OFFSET - OFFSETOF(Per_cpu, self))));
        return res;
    }

    template <typename T, size_t OFFSET> static void set_field(T value)
    {
        asm("mov %1, %%gs:%0" : "=m"(*reinterpret_cast<T*>(OFFSET - OFFSETOF(Per_cpu, self))) : "r"(value));
    }

    static void set_sys_entry_stack(void* es)
    {
        set_field<decltype(Per_cpu::sys_entry_stack), OFFSETOF(Per_cpu, sys_entry_stack)>(es);
    }
};

// Helper macros for the accessor macros below.
#define CPULOCAL_FIELD_NAME(prefix, field) prefix##_##field
#define CPULOCAL_TYPE(prefix, field) decltype(Per_cpu::CPULOCAL_FIELD_NAME(prefix, field))

// Generate a static method to read or write a CPU-local variable.
//
// The macro takes prefix and field parameters and expects that a prefix_field
// variable exists in struct Per_cpu.
//
// This macro generates an accessor function with the same name as field that
// returns a reference to the CPU-local variable.
#define CPULOCAL_ACCESSOR(prefix, field)                                                                     \
    static auto field()->CPULOCAL_TYPE(prefix, field)&                                                       \
    {                                                                                                        \
        return Cpulocal::get().CPULOCAL_FIELD_NAME(prefix, field);                                           \
    }

// Generate static methods to read or write a CPU-local variable from a local or
// remote core.
//
// This macro generates three methods:
//
// - A method with the name of field for CPU-local access (see CPULOCAL_ACCESSOR).
//
// - A method with the name remote_ref_<field> that returns a pointer to a
//   CPU-local variable. The caller is responsible to ensure proper
//   synchronization for the access to the returned reference.
//
// - A method with the name remote_load_<field> that does a
//   sequentially-consistent load of the remote value.
//
// Note: The templated definition of remote_load_<field> is necessary to make
// the definition disappear, if it would be malformed (i.e. the type is not
// compatible with Atomic::load).
#define CPULOCAL_REMOTE_ACCESSOR(prefix, field)                                                              \
    static auto remote_ref_##field(unsigned cpu)->CPULOCAL_TYPE(prefix, field)&                              \
    {                                                                                                        \
        return Cpulocal::get_remote(cpu).CPULOCAL_FIELD_NAME(prefix, field);                                 \
    }                                                                                                        \
                                                                                                             \
    template <typename T = void> static auto remote_load_##field(unsigned cpu)->CPULOCAL_TYPE(prefix, field) \
    {                                                                                                        \
        return Atomic::load(remote_ref_##field(cpu));                                                        \
    }                                                                                                        \
                                                                                                             \
    CPULOCAL_ACCESSOR(prefix, field)

// Same as CPULOCAL_ACCESSOR, but generates a (slightly more efficient)
// read-only accessor.
#define CPULOCAL_CONST_ACCESSOR(prefix, field)                                                               \
    static auto field()->CPULOCAL_TYPE(prefix, field)                                                        \
    {                                                                                                        \
        return Cpulocal::get_field<CPULOCAL_TYPE(prefix, field),                                             \
                                   OFFSETOF(Per_cpu, CPULOCAL_FIELD_NAME(prefix, field))>();                 \
    }
