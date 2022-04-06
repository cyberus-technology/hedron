/*
 * Virtual LAPIC Page
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

#pragma once

#include "memory.hpp"
#include "types.hpp"

class Vlapic
{
private:
    char opaque_data[PAGE_SIZE];

public:
    static void* operator new(size_t size);
    static void operator delete(void* ptr);
};
