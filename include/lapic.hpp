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

#include "compiler.hpp"
#include "memory.hpp"
#include "msr.hpp"
#include "x86.hpp"

class Lapic
{
    private:
        enum Register
        {
            LAPIC_IDR       = 0x2,
            LAPIC_LVR       = 0x3,
            LAPIC_TPR       = 0x8,
            LAPIC_PPR       = 0xa,
            LAPIC_EOI       = 0xb,
            LAPIC_LDR       = 0xd,
            LAPIC_DFR       = 0xe,
            LAPIC_SVR       = 0xf,
            LAPIC_ISR       = 0x10,
            LAPIC_TMR       = 0x18,
            LAPIC_IRR       = 0x20,
            LAPIC_ESR       = 0x28,
            LAPIC_ICR_LO    = 0x30,
            LAPIC_ICR_HI    = 0x31,
            LAPIC_LVT_TIMER = 0x32,
            LAPIC_LVT_THERM = 0x33,
            LAPIC_LVT_PERFM = 0x34,
            LAPIC_LVT_LINT0 = 0x35,
            LAPIC_LVT_LINT1 = 0x36,
            LAPIC_LVT_ERROR = 0x37,
            LAPIC_TMR_ICR   = 0x38,
            LAPIC_TMR_CCR   = 0x39,
            LAPIC_TMR_DCR   = 0x3e,
            LAPIC_IPI_SELF  = 0x3f,
        };

        enum Delivery_mode
        {
            DLV_FIXED       = 0U << 8,
            DLV_NMI         = 4U << 8,
            DLV_INIT        = 5U << 8,
            DLV_SIPI        = 6U << 8,
            DLV_EXTINT      = 7U << 8,
        };

        enum Shorthand
        {
            DSH_NONE        = 0U << 18,
            DSH_EXC_SELF    = 3U << 18,
        };

        static inline uint32 read (Register reg)
        {
            return *reinterpret_cast<uint32 volatile *>(CPU_LOCAL_APIC + (reg << 4));
        }

        static inline void write (Register reg, uint32 val)
        {
            *reinterpret_cast<uint32 volatile *>(CPU_LOCAL_APIC + (reg << 4)) = val;
        }

        static inline void set_lvt (Register reg, Delivery_mode dlv, unsigned vector, unsigned misc = 0)
        {
            write (reg, misc | dlv | vector);
        }

        static inline void timer_handler();

        static inline void error_handler();

        static inline void perfm_handler();

        static inline void therm_handler();

        static inline void park_handler();

    public:
        static unsigned freq_tsc;
        static unsigned freq_bus;
        static bool     use_tsc_timer;

        // Number of CPUs that still need to be parked.
        //
        // See park_all_but_self.
        static unsigned cpu_park_count;

        using park_fn = void (*)();

        /// The function to be executed before CPUs are parked.
        ///
        /// \see park_all_but_self
        static inline park_fn park_function = nullptr;

        static inline unsigned id()
        {
            return read (LAPIC_IDR) >> 24 & 0xff;
        }

        // This is a special version of id() that already works when the LAPIC
        // is not mapped yet.
        static inline unsigned early_id()
        {
            uint32 ebx, dummy;

            cpuid (1, dummy, ebx, dummy, dummy);

            return ebx >> 24; // APIC ID is encoded in bits 31 to 24.
        }

        static inline unsigned version()
        {
            return read (LAPIC_LVR) & 0xff;
        }

        static inline unsigned lvt_max()
        {
            return read (LAPIC_LVR) >> 16 & 0xff;
        }

        static inline void eoi()
        {
            write (LAPIC_EOI, 0);
        }

        static inline void set_timer (uint64 tsc)
        {
            if (not use_tsc_timer) {
                uint64 now = rdtsc();
                uint32 icr;
                write (LAPIC_TMR_ICR, tsc > now && (icr = static_cast<uint32>(tsc - now) / (freq_tsc / freq_bus)) > 0 ? icr : 1);
            } else
                Msr::write (Msr::IA32_TSC_DEADLINE, tsc);
        }

        static inline unsigned get_timer()
        {
            return read (LAPIC_TMR_CCR);
        }

        static void init();

        INIT
        static void setup();

        // Copy the AP boot code into low-memory.
        //
        // This needs to be called before APs are booted.
        static void prepare_ap_boot();

        // Restore low-memory that was clobbered during AP bringup.
        //
        // This needs to be called once all APs have successfully booted.
        static void restore_low_memory();

        static void send_ipi (unsigned, unsigned, Delivery_mode = DLV_FIXED, Shorthand = DSH_NONE);

        // Stop all CPUs except the current one.
        //
        // Parked CPUs execute the passed function and all but the calling CPU
        // enter a CLI/HLT loop.
        //
        /// This function is not safe to be called concurrently.
        static void park_all_but_self(park_fn fn);

        REGPARM (1)
        static void lvt_vector (unsigned) asm ("lvt_vector");

        REGPARM (1)
        static void ipi_vector (unsigned) asm ("ipi_vector");
};
