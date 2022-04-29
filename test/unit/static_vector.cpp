/*
 * Static vector tests
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "construct_counter.hpp"
#include <static_vector.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Size functions work", "[static_vector]")
{
    Static_vector<int, 10> v;

    CHECK(v.size() == 0);
    CHECK(v.max_size() == 10);

    v.push_back(5);
    CHECK(v.size() == 1);
}

TEST_CASE("Array access works", "[static_vector]")
{
    Static_vector<int, 10> v;

    v.push_back(1);
    v.push_back(9);

    CHECK(v[0] == 1);
    CHECK(v[1] == 9);
}

TEST_CASE("Resizing arrays works", "[static_vector]")
{
    Static_vector<int, 10> v;

    // The growth case.
    v.resize(2);

    CHECK(v.size() == 2);
    CHECK(v[0] == 0);
    CHECK(v[1] == 0);

    // The growth with different value case.
    v.resize(3, 17);
    CHECK(v[2] == 17);

    // The shrink case.
    v.resize(1);
    CHECK(v.size() == 1);
}

TEST_CASE("Construction and destruction works", "[static_vector]")
{
    SECTION("Emplace constructs once")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        Static_vector<test_counter, 10> v;

        v.emplace_back();
        CHECK(test_counter::constructed == 1);
        CHECK(test_counter::destructed == 0);
    }

    SECTION("Reset destructs")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        Static_vector<test_counter, 10> v;

        v.emplace_back();
        v.emplace_back();

        v.reset();
        CHECK(test_counter::destructed == 2);
    }

    SECTION("Resize constructs while growing")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        Static_vector<test_counter, 10> v;

        auto& constructed{test_counter::constructed};

        CHECK(test_counter::constructed == 0);
        CHECK(test_counter::destructed == 0);

        v.resize(2);

        // One extra construction/destruction comes from the default argument of resize.
        CHECK(test_counter::constructed == 3);
        CHECK(test_counter::destructed == 1);
    }

    SECTION("Resize destructs while shrinking")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        Static_vector<test_counter, 10> v;

        v.emplace_back();
        v.emplace_back();
        v.emplace_back();

        CHECK(test_counter::constructed == 3);
        CHECK(test_counter::destructed == 0);

        v.resize(1);

        // One extra destruction comes from the default argument of resize.
        CHECK(test_counter::constructed == 4);
        CHECK(test_counter::destructed == 3);
    }

    SECTION("Destructor destructs")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        {
            Static_vector<test_counter, 10> v;
            v.emplace_back();
            v.emplace_back();
        }

        CHECK(test_counter::destructed == 2);
    }
}
