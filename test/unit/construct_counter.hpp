/*
 * Constructor/Destructor call counting
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#pragma once

#include "types.hpp"

// Count calls to the constructor and destructor of this class.
//
// This is a useful helper to test whether library code properly frees
// resources.
//
// We use a tagged class to get distinct counters. To use this, create an empty
// tag class in a local scope.
template <typename TAG> class construct_counter
{
public:
    inline static size_t constructed;
    inline static size_t destructed;

    construct_counter() { constructed++; }
    ~construct_counter() { destructed++; }
};
