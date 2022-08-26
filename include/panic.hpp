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

#pragma once

#include "compiler.hpp"

#if __STDC_HOSTED__

// For hosted builds, we have a header-only implementation that makes our testing easier, because we don't
// need to pull in panic.cpp.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

[[noreturn]] FORMAT(1, 2) inline void panic(char const* format, ...)
{
    va_list ap;
    va_start(ap, format);

    vfprintf(stderr, format, ap);
    abort();

    va_end(ap);
}

#else

// An unrecoverable error has occurred and Hedron execution cannot continue.
[[noreturn]] COLD FORMAT(1, 2) void panic(char const* format, ...);

#endif
