/*
 * Algorithm Tests
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "algorithm.hpp"

TEST_CASE("accumulate works")
{
    std::vector<int> const empty;
    std::vector<int> const example{1, 2, 3};

    CHECK(::accumulate(std::begin(empty), std::end(empty), 0) == 0);
    CHECK(::accumulate(std::begin(empty), std::end(empty), 17) == 17);

    CHECK(::accumulate(std::begin(example), std::end(example), 0) == 6);
    CHECK(::accumulate(std::begin(example), std::end(example), 17) == 23);
}

TEST_CASE("find_if works")
{
    std::vector<int> empty;
    std::vector<int> example{1, 2, 3};

    auto is_even{[](int i) { return i % 2 == 0; }};

    CHECK(::find_if(std::begin(empty), std::end(empty), is_even) == std::end(empty));
    CHECK(::find_if(std::begin(example), std::end(example), is_even) == ++std::begin(example));
}

TEST_CASE("for_each works")
{
    std::vector<int> const example{1, 2, 3};
    size_t pos{0};

    ::for_each(std::begin(example), std::end(example), [&](int v) { CHECK(example[pos++] == v); });
}
