/*
 * Event Counters
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "config.hpp"
#include "console_vga.hpp"
#include "cpu.hpp"

class Counter
{
    public:
        CPULOCAL_ACCESSOR(counter, ipi);
        CPULOCAL_ACCESSOR(counter, lvt);
        CPULOCAL_ACCESSOR(counter, gsi);
        CPULOCAL_ACCESSOR(counter, exc);
        CPULOCAL_ACCESSOR(counter, vmi);
        CPULOCAL_ACCESSOR(counter, schedule);
        CPULOCAL_ACCESSOR(counter, helping);
        CPULOCAL_ACCESSOR(counter, cycles_idle);

        static void dump();

        static inline unsigned remote (unsigned cpu, unsigned ipi)
        {
            return ACCESS_ONCE(Cpulocal::get_remote(cpu).counter_ipi[ipi]);
        }

        template <unsigned D, unsigned B>
        static void print (mword val, Console_vga::Color c, unsigned col)
        {
            if (EXPECT_FALSE (Cpu::row()))
                for (unsigned i = 0; i < D; i++, val /= B)
                    Console_vga::con.put (Cpu::row(), col - i, c, !i || val ? (val % B)["0123456789ABCDEF"] : ' ');
        }
};
