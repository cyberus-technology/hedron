/*
 * String Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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
#include "types.hpp"

#if __STDC_HOSTED__
#include <cstring>
#else

extern "C" NONNULL void* memcpy(void* d, void const* s, size_t n);
extern "C" NONNULL void* memmove(void* d, void const* s, size_t n);

extern "C" NONNULL void* memset(void* d, int c, size_t n);

#endif // __STDC_HOSTED__

/// Check whether the first n bytes in two strings match.
bool strnmatch(char const* s1, char const* s2, size_t n);

// Expands to the file name without path components. Does this at compile time.
// See https://blog.galowicz.de/2016/02/20/short_file_macro/
#define FILENAME                                                                                             \
    ({                                                                                                       \
        constexpr const char* const sf__{past_last_slash(__FILE__)};                                         \
        sf__;                                                                                                \
    })

// Compile-time C-string search. Returns the component after the last slash.
// Useful to get the filename of a path.
static constexpr const char* past_last_slash(const char* const str, const char* const last_slash)
{
    return *str == '\0'  ? last_slash
           : *str == '/' ? past_last_slash(str + 1, str + 1)
                         : past_last_slash(str + 1, last_slash);
}

static constexpr const char* past_last_slash(const char* const str) { return past_last_slash(str, str); }
