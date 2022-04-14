/*
 * Generic Bitmap
 *
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "atomic.hpp"
#include "math.hpp"
#include "string.hpp"
#include "types.hpp"

#if __STDC_HOSTED__
#include <iterator>
#endif

/**
 * Simple Generic Bitmap
 *
 * Stores a given number of bits in an underlying array of the type T.
 */
template <typename T, size_t NUMBER_OF_BITS> class Bitmap
{

    static constexpr size_t BITS_PER_WORD{sizeof(T) * 8};

    static size_t word_index(size_t i) { return i / BITS_PER_WORD; }
    static size_t bit_index(size_t i) { return i % BITS_PER_WORD; }
    static T bit_mask(size_t i) { return static_cast<T>(1) << bit_index(i); }

    static constexpr size_t WORDS{align_up(NUMBER_OF_BITS, BITS_PER_WORD) / BITS_PER_WORD};
    T bitmap_[WORDS];
    static_assert(sizeof(bitmap_) * 8 >= NUMBER_OF_BITS, "Bitmap backing store too small for requests size");

public:
    using value_type = T;

    /// Helper to simulate a bool reference
    class Bit_accessor
    {
    public:
        Bit_accessor() = delete;

        Bit_accessor(Bitmap& bitmap, size_t pos) : bitmap_(bitmap), pos_(pos)
        {
            assert(pos < NUMBER_OF_BITS);
        }

        /// Assigns val to the corresponding bit
        void operator=(bool val) { bitmap_.set(pos_, val); }

        bool atomic_fetch() const { return bitmap_.atomic_fetch(pos_); }

        bool atomic_fetch_set() { return bitmap_.atomic_fetch_set(pos_); }

        void atomic_clear() { bitmap_.atomic_clear(pos_); }

        operator bool() const { return bitmap_.get(pos_); }

    private:
        Bitmap& bitmap_;
        size_t pos_;
    };

    /// A simple forward iterator for the bitmap class.
    class Iterator
    {
    public:
        using value_type = bool;
        using difference_type = ptrdiff_t;
        using pointer = void;
        using reference = Bit_accessor;

#if __STDC_HOSTED__
        using iterator_category = std::forward_iterator_tag;
#endif

        Iterator(Bitmap& bitmap, size_t pos) : bitmap_(bitmap), pos_(pos)
        {
            // The "equal" case here handles the special
            // one-past-the-end end() iterator.
            assert(pos <= NUMBER_OF_BITS);
        }

        Bit_accessor operator*() { return Bit_accessor{bitmap_, pos_}; }

        Iterator operator++()
        {
            pos_++;

            return *this;
        }

        bool operator==(Iterator const& other) const
        {
            assert(&bitmap_ == &other.bitmap_);
            return pos_ == other.pos_;
        }

        bool operator!=(Iterator const& other) const { return not(*this == other); }

    private:
        Bitmap& bitmap_;
        size_t pos_;
    };

    explicit Bitmap(bool initial_value) { memset(bitmap_, initial_value * 0xFF, sizeof(bitmap_)); }

    /// Return an iterator to the first bit of the bitmap.
    Iterator begin() { return {*this, 0}; }

    /// Return an iterator to the end bit of the bitmap.
    Iterator end() { return {*this, NUMBER_OF_BITS}; }

    /// Return the size in bits of the bitmap.
    static size_t size() { return NUMBER_OF_BITS; }

    /// Obtain a bool-reference like object to access bit idx.
    Bit_accessor operator[](size_t i) { return {*this, i}; }

    /// Set the given bit to a specified value.
    void set(size_t i, bool v)
    {
        assert(i < NUMBER_OF_BITS);
        bitmap_[word_index(i)] &= ~bit_mask(i);
        bitmap_[word_index(i)] |= v ? bit_mask(i) : 0;
    }

    /// Return the bit value at a specified position.
    bool get(size_t i) const
    {
        assert(i < NUMBER_OF_BITS);
        return bitmap_[word_index(i)] & bit_mask(i);
    }

    /// Atomically set a bit in the bitmap and return its old value.
    bool atomic_fetch(size_t i) const
    {
        assert(i < NUMBER_OF_BITS);
        return Atomic::load(bitmap_[word_index(i)]) & bit_mask(i);
    }

    /// Atomically set a bit in the bitmap and return its old value.
    bool atomic_fetch_set(size_t i)
    {
        assert(i < NUMBER_OF_BITS);
        return Atomic::test_set_bit(bitmap_[word_index(i)], bit_index(i));
    }

    /// Atomically clear a single bit in the bitmap.
    void atomic_clear(size_t i)
    {
        assert(i < NUMBER_OF_BITS);
        Atomic::clr_mask(bitmap_[word_index(i)], bit_mask(i));
    }

    /// Atomically merge two bitmaps.
    ///
    /// Note: This function is safe to be called concurrently, i.e. it is
    /// safe to merge into a single bitmap from different CPUs, _but_ it
    /// will do updates word-by-word. This means that an observer can see
    /// partial results.
    void atomic_union(Bitmap const& other)
    {
        for (size_t i = 0; i < WORDS; i++) {
            Atomic::set_mask(bitmap_[i], other.bitmap_[i]);
        }
    }
};
