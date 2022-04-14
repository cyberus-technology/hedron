/*
 * Generic Console
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

#include "compiler.hpp"
#include "types.hpp"
#include <stdarg.h>

class Spinlock;

class Console
{
private:
    enum
    {
        MODE_FLAGS = 0,
        MODE_WIDTH = 1,
        MODE_PRECS = 2,
        FLAG_SIGNED = 1UL << 0,
        FLAG_ALT_FORM = 1UL << 1,
        FLAG_ZERO_PAD = 1UL << 2,
    };

    Console* next;

    static Console* list;
    static Spinlock lock;

    virtual void putc(int) = 0;
    void print_num(uint64, unsigned, unsigned, unsigned);
    void print_str(char const*, unsigned, unsigned);

    FORMAT(2, 0)
    void vprintf(char const*, va_list);

protected:
    NOINLINE
    void enable()
    {
        Console** ptr;

        for (ptr = &list; *ptr; ptr = &(*ptr)->next) {
        }

        *ptr = this;
    }

public:
    FORMAT(1, 2)
    static void print(char const*, ...);

    FORMAT(1, 2) NORETURN static void panic(char const*, ...);
};
