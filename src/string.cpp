/*
 * String Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "string.hpp"
#include "string_impl.hpp"

// USED attributes are important to prevent linker failures when link-time
// optimization is enabled.

USED void* memcpy(void* d, void const* s, size_t n) { return impl_memcpy(d, s, n); }
USED void* memmove(void* d, void const* s, size_t n) { return impl_memmove(d, s, n); }

USED void* memset(void* d, int c, size_t n) { return impl_memset(d, c, n); }

bool strnmatch(char const* s1, char const* s2, size_t n) { return impl_strnmatch(s1, s2, n); }
