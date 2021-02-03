/*
 * VGA Console
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

#include "cmdline.hpp"
#include "console_vga.hpp"
#include "pd.hpp"

INIT_PRIORITY (PRIO_CONSOLE) Console_vga Console_vga::con;

Console_vga::Console_vga() : num (25), row (0), col (0)
{
    if (Cmdline::novga)
        return;

    Pd::kern->claim_mmio_page(HV_GLOBAL_FBUF, 0xb9000, false);

    set_page (1);

    enable();
}

void Console_vga::putc (int c)
{
    if (EXPECT_FALSE (c == '\f')) {
        clear_all();
        row = col = 0;
        return;
    }

    if (EXPECT_TRUE (c != '\n')) {
        put (row, col, COLOR_LIGHT_WHITE, c);
        if (EXPECT_TRUE (++col < 80))
            return;
    }

    col = 0;

    if (EXPECT_TRUE (++row == num))
        clear_row (--row);
}
