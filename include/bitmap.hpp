/*
 * Generic Bitmap
 *
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
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

#pragma once

#include "assert.hpp"
#include "math.hpp"
#include "string.hpp"
#include "types.hpp"

/**
 * Simple Generic Bitmap
 *
 * Stores a given number of bits in an underlying array of the type T.
 */
template <typename T, size_t NUMBER_OF_BITS>
class Bitmap
{
public:
    /// Helper to simulate a bool reference
    class Bit_accessor
    {
    public:
        Bit_accessor(T &raw_, size_t bitpos_) : raw(raw_), bitpos(bitpos_)
        {
            assert(bitpos_ < sizeof(T) * 8);
        }

        /// Assigns val to the corresponding bit
        void operator=(const bool val)
        {
            raw &= ~(1u << bitpos);
            raw |= val * (1u << bitpos);
        }

    private:
        T &raw;
        size_t bitpos {static_cast<size_t>(-1)};
    };

    explicit Bitmap(bool initial_value)
    {
        memset(bitmap, initial_value * 0xFF, sizeof(bitmap));
    }

    /// Obtain a bool-reference like object to access bit idx
    Bit_accessor operator[](size_t idx)
    {
        assert(idx < NUMBER_OF_BITS);
        assert(idx < sizeof(bitmap) * 8);

        return {bitmap[idx / sizeof(bitmap[0]) / 8], idx % (sizeof(bitmap[0]) * 8)};
    }

private:
    T bitmap[align_up(NUMBER_OF_BITS, sizeof(T) * 8) / 8 / sizeof(T)];
};
