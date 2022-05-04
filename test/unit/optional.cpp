/*
 * Tests for the Optional type
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
#include <optional.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Default construction works", "[optional]")
{
    Optional<int> v;

    CHECK(not v.has_value());
}

TEST_CASE("Construction with optional works", "[optional]")
{
    Optional<int> w{7};
    Optional<int> v{w};

    CHECK(v.has_value());
    CHECK(*v == 7);
}

TEST_CASE("Construction with value works", "[optional]")
{
    Optional<int> v{7};

    CHECK(v.has_value());
    CHECK(*v == 7);
}

TEST_CASE("Assignment with value works", "[optional]")
{
    Optional<int> v;

    v = 7;
    CHECK(v.has_value());
    CHECK(*v == 7);
}

TEST_CASE("Assignment with another optional works", "[optional]")
{
    Optional<int> v;

    v = Optional<int>{};
    CHECK(not v.has_value());

    v = Optional<int>{7};
    CHECK(v.has_value());
    CHECK(*v == 7);
}

TEST_CASE("Optional::value_or works", "[optional]")
{
    Optional<int> no_value;
    Optional<int> with_value{7};

    CHECK(no_value.value_or(10) == 10);
    CHECK(with_value.value_or(10) == 7);
}

TEST_CASE("Optional comparisons work", "[optional]")
{
    Optional<int> const no_value{};
    Optional<int> const value{1};
    Optional<int> const other_value{7};

    CHECK(no_value == no_value);
    CHECK(no_value != value);
    CHECK(value == value);
    CHECK(value != other_value);
}

TEST_CASE("Optional construction and destruction works", "[optional]")
{
    SECTION("Default constructor does not construct or destruct")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        Optional<test_counter> o;

        CHECK(test_counter::constructed == 0);
        CHECK(test_counter::destructed == 0);
    }

    SECTION("Empty option does not construct or destruct")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        {
            Optional<test_counter> o;
        }

        CHECK(test_counter::constructed == 0);
        CHECK(test_counter::destructed == 0);
    }

    SECTION("Full option destructs")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        {
            Optional<test_counter> o{test_counter{}};
        }

        // The temporary and the contained value are constructed and destructed.
        CHECK(test_counter::constructed == 2);
        CHECK(test_counter::destructed == 2);
    }

    SECTION("Assignment destructs")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        {
            Optional<test_counter> o{test_counter{}};

            // The temporary and the contained value is contructed.
            CHECK(test_counter::constructed == 2);

            // The temporary from initialization is already destructed.
            CHECK(test_counter::destructed == 1);

            o = test_counter();

            // Another temporary and the newly contained value are constructed.
            CHECK(test_counter::constructed == 4);

            // The temporary and the old contained value are destructed.
            CHECK(test_counter::destructed == 3);
        }
    }

    SECTION("Copy constructor constructs")
    {
        struct local_tag {
        };
        using test_counter = construct_counter<local_tag>;

        test_counter init_val;
        CHECK(test_counter::constructed == 1);

        Optional<test_counter> o{init_val};

        CHECK(test_counter::constructed == 2);
    }
}
