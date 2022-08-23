/*
 * Scope Guard Tests
 *
 * Copyright (C) 2022 Julian Stecklina, Cyberus Technology GmbH.
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

#include "scope_guard.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Scope_guard calls cleanup on destruct", "[scope_guard]")
{
    int i = 0;

    {
        Scope_guard g{[&i] { i++; }};

        CHECK(i == 0);
    }

    CHECK(i == 1);
}
