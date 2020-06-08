/*
 * Algorithm Tests
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "algorithm.hpp"

TEST_CASE("Accumulate works on arrays")
{
    std::array<int, 0> const empty_array;
    std::array<int, 3> const test_array = {1, 2, 3};

    CHECK(accumulate(empty_array.begin(), empty_array.end(), 0) == 0);
    CHECK(accumulate(empty_array.begin(), empty_array.end(), 17) == 17);

    CHECK(accumulate(test_array.begin(), test_array.end(), 0) == 6);
    CHECK(accumulate(test_array.begin(), test_array.end(), 17) == 23);
}
