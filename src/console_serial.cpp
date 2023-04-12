/*
 * Serial Console
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

#include "console_serial.hpp"
#include "cmdline.hpp"
#include "hpt.hpp"
#include "initprio.hpp"
#include "x86.hpp"

Console_serial::Console_serial()
{
    if (!Cmdline::serial)
        return;

    char* mem = static_cast<char*>(Hpt::remap(0));
    if (!(base = *reinterpret_cast<uint16*>(mem + 0x400)) &&
        !(base = *reinterpret_cast<uint16*>(mem + 0x402)))
        base = 0x3f8;

    out(LCR, 0x80);
    out(DLL, (freq / 115200) & 0xff);
    out(DLM, (freq / 115200) >> 8);
    out(LCR, 3);
    out(IER, 0);
    out(FCR, 7);
    out(MCR, 3);

    enable();
}

void Console_serial::putc(int c)
{
    if (c == '\n')
        putc('\r');

    while (EXPECT_FALSE(!(in(LSR)&0x20)))
        relax();

    out(THR, c);
}
