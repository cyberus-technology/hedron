/*
 * Panic handling
 *
 * Copyright (C) 2022 Julian Stecklina, Cyberus Technology GmbH.
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

#include "panic.hpp"

#include "console.hpp"
#include "x86.hpp"

#include <stdarg.h>

void panic(char const* format, ...)
{
    va_list ap;

    va_start(ap, format);

    Console::print("PANIC: Hedron encountered an unrecoverable error near RIP %p:",
                   __builtin_return_address(0));
    Console::vprint(format, ap);
    shutdown();

    va_end(ap);
}
