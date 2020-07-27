/*
 * String Tests
 *
 * Copyright (C) 2019 Jana Traue, Cyberus Technology GmbH.
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

#include "string_impl.hpp"

#include <array>
#include <catch2/catch.hpp>

TEST_CASE ("memcpy works", "[string]")
{
    std::array<char, 4>       dst_array = { 0, 0, 0, 0};
    std::array<char, 4> const src_array = { 1, 1, 1, 1};

    impl_memcpy (dst_array.data(), src_array.data(), dst_array.size());

    CHECK (dst_array == src_array);
}

TEST_CASE ("memmove works", "[string]")
{
    std::array<char, 4> array = { 0, 1, 2, 0};

    SECTION ("Forward copy") {
        impl_memmove (array.data(), array.data() + 1, 2);

        std::array<char, 4> const expected = { 1, 2, 2, 0};
        CHECK (array == expected);
    }

    SECTION ("Backward copy") {
        impl_memmove (array.data() + 2, array.data() + 1, 2);

        std::array<char, 4> const expected = { 0, 1, 1, 2};
        CHECK (array == expected);
    }
}

TEST_CASE ("memset works", "[string]")
{
    std::array<char, 4> array = { 1, 2, 3, 4};

    impl_memset (array.data() + 1, 9, 2);

    std::array<char, 4> const expected = { 1, 9, 9, 4};
    CHECK (array == expected);
}

TEST_CASE ("String prefix match", "[string]")
{
    char const *string { "foo bar" };
    char const *prefix { "foo xy" };
    char const *empty { "" };
    char const *empty2 { "" };

    SECTION ("prefix matches") {
        CHECK (impl_strnmatch (prefix, string, 1));
        CHECK (impl_strnmatch (prefix, string, 4));
    };

    SECTION ("prefix does not match") {
        CHECK (not impl_strnmatch (prefix, string, 5));
        CHECK (not impl_strnmatch (empty, string, 1));
    };

    SECTION ("string termination symbol is handled correctly") {
        CHECK (impl_strnmatch (empty, empty2, 1));
    };

    SECTION ("zero length prefix match") {
        CHECK (impl_strnmatch (prefix, string, 0));
    };
}
