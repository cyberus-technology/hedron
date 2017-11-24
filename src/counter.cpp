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

#include "counter.hpp"
#include "stdio.hpp"
#include "x86.hpp"

unsigned    Counter::ipi[NUM_IPI];
unsigned    Counter::lvt[NUM_LVT];
unsigned    Counter::gsi[NUM_GSI];
unsigned    Counter::exc[NUM_EXC];
unsigned    Counter::vmi[NUM_VMI];
unsigned    Counter::schedule;
unsigned    Counter::helping;
uint64      Counter::cycles_idle;

void Counter::dump()
{
    trace (0, "TIME: %16llu", rdtsc());
    trace (0, "IDLE: %16llu", Counter::cycles_idle);
    trace (0, "SCHD: %16u", Counter::schedule);
    trace (0, "HELP: %16u", Counter::helping);

    for (unsigned i = 0; i < sizeof (Counter::ipi) / sizeof (*Counter::ipi); i++)
        if (Counter::ipi[i]) {
            trace (0, "IPI %#4x: %12u", i, Counter::ipi[i]);
            Counter::ipi[i] = 0;
        }

    for (unsigned i = 0; i < sizeof (Counter::lvt) / sizeof (*Counter::lvt); i++)
        if (Counter::lvt[i]) {
            trace (0, "LVT %#4x: %12u", i, Counter::lvt[i]);
            Counter::lvt[i] = 0;
        }

    for (unsigned i = 0; i < sizeof (Counter::gsi) / sizeof (*Counter::gsi); i++)
        if (Counter::gsi[i]) {
            trace (0, "GSI %#4x: %12u", i, Counter::gsi[i]);
            Counter::gsi[i] = 0;
        }

    for (unsigned i = 0; i < sizeof (Counter::exc) / sizeof (*Counter::exc); i++)
        if (Counter::exc[i]) {
            trace (0, "EXC %#4x: %12u", i, Counter::exc[i]);
            Counter::exc[i] = 0;
        }

    for (unsigned i = 0; i < sizeof (Counter::vmi) / sizeof (*Counter::vmi); i++)
        if (Counter::vmi[i]) {
            trace (0, "VMI %#4x: %12u", i, Counter::vmi[i]);
            Counter::vmi[i] = 0;
        }
}
