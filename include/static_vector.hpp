/*
 * A vector with statically-allocated backing store
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

#pragma once

#include "assert.hpp"
#include "types.hpp"
#include "util.hpp"

// A vector with statically allocated backing store and a maximum size.
template <typename T, size_t N> class Static_vector
{
private:
    // The number of elements in the vector.
    size_t size_{0};

    // The actual backing storage for the vector elements.
    alignas(T) char backing[sizeof(T) * N];

public:
    T* data() { return reinterpret_cast<T*>(backing); }
    T const* data() const { return reinterpret_cast<T const*>(backing); }

    T& operator[](size_t i) { return data()[i]; }
    T const& operator[](size_t i) const { return data()[i]; };

    T* begin() { return &data()[0]; }
    T const* begin() const { return &data()[0]; }

    T* end() { return &data()[size()]; }
    T const* end() const { return &data()[size()]; }

    size_t size() const { return size_; };
    constexpr size_t max_size() const { return N; }

    template <typename... ARGS> void emplace_back(ARGS&&... args)
    {
        assert(size() < max_size());

        new (&data()[size_++]) T(forward<ARGS>(args)...);
    }

    void push_back(T const& o) { emplace_back(o); }

    void reset()
    {
        // We cannot call resize(0) here, because resize only works for types that are default constructible.
        for (T& elem : *this) {
            elem.~T();
        }

        size_ = 0;
    }

    void resize(size_t new_size, T const& new_value = {})
    {
        assert(new_size <= max_size());

        // Shrink the container to the given size.
        while (new_size < size()) {
            data()[--size_].~T();
        }

        // Grow the container to the given size with the given value.
        while (new_size > size()) {
            push_back(new_value);
        }

        assert(new_size == size());
    }

    ~Static_vector() { reset(); }
};
