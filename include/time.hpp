/*
 * Time related functions
 *
 * Copyright (C) 2022 Stefan Hertrampf, Cyberus Technology GmbH.
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

#include "types.hpp"

constexpr uint64 ONE_SEC_IN_US{1000u * 1000u};

// Convert a given amount of microseconds to TSC clock ticks.
inline uint64 us_as_ticks_in_freq(uint32 freq_khz, uint32 microseconds)
{
    return static_cast<uint64>(freq_khz) * microseconds / 1000;
}
