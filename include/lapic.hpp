/*
 * Local Advanced Programmable Interrupt Controller (Local APIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "compiler.hpp"
#include "memory.hpp"
#include "msr.hpp"
#include "x86.hpp"

class Lapic
{
private:
    enum Register
    {
        LAPIC_IDR = 0x2,
        LAPIC_LVR = 0x3,
        LAPIC_TPR = 0x8,
        LAPIC_PPR = 0xa,
        LAPIC_EOI = 0xb,
        LAPIC_LDR = 0xd,
        LAPIC_DFR = 0xe,
        LAPIC_SVR = 0xf,
        LAPIC_ISR = 0x10,
        LAPIC_TMR = 0x18,
        LAPIC_IRR = 0x20,
        LAPIC_ESR = 0x28,
        LAPIC_ICR_LO = 0x30,
        LAPIC_ICR_HI = 0x31,
        LAPIC_LVT_TIMER = 0x32,
        LAPIC_LVT_THERM = 0x33,
        LAPIC_LVT_PERFM = 0x34,
        LAPIC_LVT_LINT0 = 0x35,
        LAPIC_LVT_LINT1 = 0x36,
        LAPIC_LVT_ERROR = 0x37,
        LAPIC_TMR_ICR = 0x38,
        LAPIC_TMR_CCR = 0x39,
        LAPIC_TMR_DCR = 0x3e,
        LAPIC_IPI_SELF = 0x3f,
    };

    enum Delivery_mode
    {
        DLV_FIXED = 0U << 8,
        DLV_NMI = 4U << 8,
        DLV_INIT = 5U << 8,
        DLV_SIPI = 6U << 8,
        DLV_EXTINT = 7U << 8,
    };

    enum Shorthand
    {
        DSH_NONE = 0U << 18,
        DSH_SELF = 1U << 18,
        DSH_EXC_SELF = 3U << 18,
    };

    enum Mask
    {
        MASKED = 1U << 16,
    };

    static inline uint32 read(Register reg)
    {
        return *reinterpret_cast<uint32 volatile*>(CPU_LOCAL_APIC + (reg << 4));
    }

    static inline void write(Register reg, uint32 val)
    {
        *reinterpret_cast<uint32 volatile*>(CPU_LOCAL_APIC + (reg << 4)) = val;
    }

public:
    static unsigned freq_tsc;

    // Number of CPUs that still need to be parked.
    //
    // See park_all_but_self.
    static unsigned cpu_park_count;

    using park_fn = void (*)();

    /// The function to be executed before CPUs are parked.
    ///
    /// \see park_all_but_self
    static inline park_fn park_function = nullptr;

    // Prepares a CPU to be parked and parks it.
    [[noreturn]] static void park_handler();

    static inline unsigned id() { return read(LAPIC_IDR) >> 24 & 0xff; }

    // This is a special version of id() that already works when the LAPIC
    // is not mapped yet.
    static inline unsigned early_id()
    {
        uint32 ebx, dummy;

        cpuid(1, dummy, ebx, dummy, dummy);

        return ebx >> 24; // APIC ID is encoded in bits 31 to 24.
    }

    static inline unsigned version() { return read(LAPIC_LVR) & 0xff; }

    static inline unsigned lvt_max() { return read(LAPIC_LVR) >> 16 & 0xff; }

    static void init();

    static void setup();

    enum class cpu_boot_type
    {
        AP,
        BSP
    };

    // Copy the CPU boot code into low-memory.
    //
    // This can be used to prepare either AP boot during normal boot and
    // resume. It can also be used to prepare BSP boot during resume.
    //
    // Returns the physical memory location where the boot code was placed.
    static uint32 prepare_cpu_boot(cpu_boot_type type);

    // Restore low-memory that was clobbered during CPU bringup.
    //
    // This is the counterpart to prepare_cpu_boot.
    static void restore_low_memory();

    // Send an IPI with the given vector to the given CPU.
    static void send_ipi(unsigned cpu, unsigned vector, Delivery_mode = DLV_FIXED, Shorthand = DSH_NONE);

    // Send an NMI to the given CPU. If the CPU has the might_loose_nmis flag set to true, this function will
    // not send an NMI and return false. Otherwise returns true.
    static bool send_nmi(unsigned cpu);

    // Stop all CPUs except the current one.
    //
    // Parked CPUs execute the passed function and all but the calling CPU
    // enter a CLI/HLT loop.
    //
    /// This function is not safe to be called concurrently.
    static void park_all_but_self(park_fn fn);

    // This function is called if Hedron receives an interrupt. This should not happen, thus we handle it by
    // panicking.
    [[noreturn]] REGPARM(1) static void handle_interrupt(unsigned vector) asm("handle_interrupt");
};
