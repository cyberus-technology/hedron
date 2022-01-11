/*
 * Math Function Tests
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include <catch2/catch.hpp>

#include "math.hpp"

TEST_CASE("Minimum is computed", "[math]")
{
    CHECK(min<int>(-1, 2) == -1);
    CHECK(min<int>(2, -1) == -1);
    CHECK(min<int>(2, 2) == 2);
}

TEST_CASE("Maximum is computed", "[math]")
{
    CHECK(max<int>(-1, 2) == 2);
    CHECK(max<int>(2, -1) == 2);
    CHECK(max<int>(2, 2) == 2);
}

TEST_CASE("Bit scans work", "[math]")
{
    SECTION("zero is handled correctly")
    {
        CHECK(bit_scan_forward(0) == -1);
        CHECK(bit_scan_reverse(0) == -1);
    }

    SECTION("normal bit scans work")
    {
        CHECK(bit_scan_forward(1 << 4 | 1 << 3) == 3);
        CHECK(bit_scan_reverse(1 << 4 | 1 << 3) == 4);
    }
}

TEST_CASE("Finding maximum order works", "[math]")
{
    CHECK(max_order(0, 0) == -1);
    CHECK(max_order(0, 1 << 4) == 4);
    CHECK(max_order(1 << 2, 1 << 4) == 2);
}

TEST_CASE("Alignment functions work", "[math]")
{
    CHECK(align_dn(0x4000, 0x1000) == 0x4000);
    CHECK(align_dn(0x4005, 0x1000) == 0x4000);

    CHECK(align_up(0x4000, 0x1000) == 0x4000);
    CHECK(align_up(0x4005, 0x1000) == 0x5000);
}
