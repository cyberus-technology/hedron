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

#include <catch2/catch.hpp>

#include "string.hpp"

TEST_CASE ("String prefix match")
{
    char const *string { "foo bar" };
    char const *prefix { "foo xy" };
    char const *empty { "" };
    char const *empty2 { "" };

    SECTION ("prefix matches") {
        CHECK (strnmatch (prefix, string, 1));
        CHECK (strnmatch (prefix, string, 4));
    };

    SECTION ("prefix does not match") {
        CHECK (not strnmatch (prefix, string, 5));
        CHECK (not strnmatch (empty, string, 1));
    };

    SECTION ("string termination symbol is handled correctly") {
        CHECK (strnmatch (empty, empty2, 1));
    };

    SECTION ("zero length prefix match") {
        CHECK (strnmatch (prefix, string, 0));
    };
}
