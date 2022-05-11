/*
 * Global System Interrupts (GSI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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
#include "config.hpp"

class Ioapic;
class Sm;

class Gsi
{
public:
    Sm* sm;
    Ioapic* ioapic;

    static Gsi gsi_table[NUM_GSI];

    static void setup();

    static void configure_ioapic_irt(unsigned gsi, unsigned cpu, bool level, bool active_low);
    static uint64 configure_msi(unsigned gsi, unsigned cpu, unsigned rid);

    static void unmask(unsigned gsi);

    REGPARM(1)
    static void vector(unsigned) asm("gsi_vector");
};
