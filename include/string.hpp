/*
 * String Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "compiler.hpp"
#include "types.hpp"

#if __STDC_HOSTED__
#include <cstring>
#else

extern "C" NONNULL void *memcpy  (void *d, void const *s, size_t n);
extern "C" NONNULL void *memmove (void *d, void const *s, size_t n);

extern "C" NONNULL void *memset  (void *d, int c, size_t n);

#endif // __STDC_HOSTED__

/// Check whether the first n bytes in two strings match.
bool strnmatch (char const *s1, char const *s2, size_t n);
