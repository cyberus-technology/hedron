/*
 * Time tests
 *
 * Copyright (C) 2022 Stefan Hertrampf, Cyberus Technology GmbH.
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

// Include the class under test first to detect any missing includes early
#include "time.hpp"

#include <catch2/catch.hpp>
#include <chrono>

TEST_CASE("us_to_ticks_works", "[time]")
{
    // Use some real world TSC frequency
    uint32 freq_tsc_khz = 0x2f3ec2;

    CHECK(us_as_ticks_in_freq(freq_tsc_khz, 0) == 0);
    CHECK(us_as_ticks_in_freq(freq_tsc_khz, 1000) == freq_tsc_khz);

    // No overflow when using max values
    CHECK(us_as_ticks_in_freq(~0u, ~0u) == 0x4189374b439581);
}
