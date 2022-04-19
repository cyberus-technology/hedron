/*
 * CPU information
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

enum class Cpu_vendor : unsigned
{
    UNKNOWN,
    INTEL,
    AMD,
};

struct Cpu_info {
    unsigned package;
    unsigned core;
    unsigned thread;

    Cpu_vendor vendor;
    unsigned platform;
    unsigned family;
    unsigned model;
    unsigned stepping;
    unsigned brand;
    unsigned patch;

    uint32 name[12];
};
