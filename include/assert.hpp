/*
 * Assertions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#if __STDC_HOSTED__

// In hosted builds (unit tests), all assertions map to the libc assert macro.

#include <assert.h>
#define assert_slow assert

#else

// In non-hosted builds, we always compile assertions unless they are marked as
// slow. These assertions will not be included in release builds for performance
// reasons.

#include "console.hpp"

#define assert(X)                                                                                            \
    do {                                                                                                     \
        if (EXPECT_FALSE(!(X))) {                                                                            \
            Console::panic("Assertion \"%s\" failed at %s:%d:%s", #X, __FILE__, __LINE__,                    \
                           __PRETTY_FUNCTION__);                                                             \
        }                                                                                                    \
    } while (0)

#ifdef NDEBUG
#define assert_slow(X)                                                                                       \
    do {                                                                                                     \
    } while (0)
#else
#define assert_slow(X) assert(X)
#endif

#endif // __STDC_HOSTED__
