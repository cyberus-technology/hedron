/*
 * Unique pointer tests
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

#include <unique_ptr.hpp>
#include "construct_counter.hpp"

#include <catch2/catch.hpp>

TEST_CASE ("Boolean conversion works", "[unique_ptr]")
{
    Unique_ptr<int> const int_empty_ptr;
    Unique_ptr<int> const int_full_ptr {make_unique<int>()};

    CHECK_FALSE(int_empty_ptr);
    CHECK(int_full_ptr);
}

TEST_CASE ("Simple usage works", "[unique_ptr]")
{
    struct local_tag {};
    using test_counter = construct_counter<local_tag>;

    {
        auto ptr {make_unique<test_counter>()};

        CHECK(test_counter::constructed == 1);
        CHECK(test_counter::destructed  == 0);
    }

    CHECK(test_counter::constructed == 1);
    CHECK(test_counter::destructed  == 1);
}

TEST_CASE ("Reset frees memory", "[unique_ptr]")
{
    struct local_tag {};
    using test_counter = construct_counter<local_tag>;

    auto ptr {make_unique<test_counter>()};

    CHECK(test_counter::destructed  == 0);

    ptr.reset();

    CHECK(test_counter::destructed  == 1);
}

TEST_CASE ("Release does not free memory", "[unique_ptr]")
{
    struct local_tag {};
    using test_counter = construct_counter<local_tag>;

    auto ptr {make_unique<test_counter>()};

    CHECK(test_counter::destructed  == 0);

    auto *naked_ptr {ptr.release()};

    CHECK(test_counter::destructed  == 0);

    delete naked_ptr;
}

TEST_CASE("Move construction works", "[unique_ptr]")
{
    auto ptr_source {make_unique<int>(1)};
    auto ptr_target {std::move(ptr_source)};

    CHECK_FALSE(ptr_source);
    CHECK(*ptr_target == 1);
}


TEST_CASE("Assignment works", "[unique_ptr]")
{
    auto ptr_source {make_unique<int>(1)};
    auto ptr_target {make_unique<int>(2)};

    ptr_target = std::move(ptr_source);

    CHECK_FALSE(ptr_source);
    CHECK(*ptr_target == 1);
}

TEST_CASE("Assignment frees memory", "[unique_ptr]")
{
    struct local_tag {};
    using test_counter = construct_counter<local_tag>;

    auto ptr_target {make_unique<test_counter>()};
    auto ptr_source {make_unique<test_counter>()};

    CHECK(test_counter::destructed  == 0);

    ptr_target = std::move(ptr_source);

    CHECK(test_counter::destructed  == 1);
}
