/*
 * Serial Console
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "console.hpp"
#include "io.hpp"

class Console_serial : public Console
{
private:
    enum Register
    {
        THR = 0, // Transmit Holding Register
        IER = 1, // Interrupt Enable Register
        FCR = 2, // FIFO Control Register
        LCR = 3, // Line Control Register
        MCR = 4, // Modem Control Register
        LSR = 5, // Line Status Register
        DLL = 0, // Divisor Latch (LSB)
        DLM = 1, // Divisor Latch (MSB)
    };

    static unsigned const freq = 115200;

    unsigned base;

    inline unsigned in(Register r) { return Io::in<uint8>(base + r); }

    inline void out(Register r, unsigned v) { Io::out(base + r, static_cast<uint8>(v)); }

    void putc(int c);

public:
    Console_serial();

    static Console_serial con;
};
