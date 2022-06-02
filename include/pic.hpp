/*
 * Programmable Interrupt Controller (PIC) Support
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "io.hpp"
#include "vectors.hpp"

class Pic
{
public:
    static void init()
    {
        // Start initialization sequence.
        Io::out<uint8>(0x20, 0x11);

        // Program interrupt vector offset.
        //
        // As we mask the PIC below, we should never get this interrupt.
        Io::out<uint8>(0x21, VEC_USER);

        // Slave PIC at IRQ2.
        Io::out<uint8>(0x21, 0x4);

        // 8086 Mode.
        Io::out<uint8>(0x21, 0x1);

        // Mask all interrupts.
        Io::out<uint8>(0x21, 0xff);

        // We don't need to touch the slave controller because its cascade
        // IRQ is masked in the master PIC.
    }
};
