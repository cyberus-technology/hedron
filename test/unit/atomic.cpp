/*
 * Atomic Tests
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include <catch2/catch.hpp>

#include "atomic.hpp"

TEST_CASE("Atomic read-modify-write operations return new value")
{
    const int old_value{128};
    int value_to_modify{old_value};

    SECTION("add") { CHECK(Atomic::add(value_to_modify, 1) == old_value + 1); };
    SECTION("sub") { CHECK(Atomic::sub(value_to_modify, 1) == old_value - 1); };
}
