/*
 * A wrapper for optional values.
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

#pragma once

#include "assert.hpp"
#include "util.hpp"

// A wrapper for optionally contained values.
//
// This type works similarly to std::optional, but avoids the implicit bool conversions for has_value().
template <typename T> class Optional
{
private:
    alignas(T) char backing[sizeof(T)];

    bool has_value_{false};

    T* data() { return reinterpret_cast<T*>(backing); }
    T const* data() const { return reinterpret_cast<T const*>(backing); }

    void destroy_value()
    {
        if (has_value_) {
            data()->~T();
            has_value_ = false;
        }
    }

public:
    bool has_value() const { return has_value_; }

    Optional() = default;

    Optional(Optional const& init_val) : has_value_(init_val.has_value())
    {
        if (has_value_) {
            new (data()) T(*init_val);
        }
    }

    Optional(T const& init_val) : has_value_(true) { new (data()) T(init_val); }

    ~Optional() { destroy_value(); }

    T& value()
    {
        assert(has_value());
        return *data();
    }

    T const& value() const
    {
        assert(has_value());
        return *data();
    }

    template <typename U> T value_or(U&& other) const
    {
        if (has_value()) {
            return value();
        } else {
            return other;
        }
    }

    T& operator*() { return value(); }

    T const& operator*() const { return value(); }

    T* operator->()
    {
        assert(has_value());
        return data();
    }

    T const* operator->() const
    {
        assert(has_value());
        return data();
    }

    Optional& operator=(T const& rhs)
    {
        destroy_value();

        new (data()) T(rhs);
        has_value_ = true;

        return *this;
    }

    Optional& operator=(Optional const& rhs)
    {
        destroy_value();

        if (rhs.has_value()) {
            new (data()) T(*rhs);
            has_value_ = true;
        }

        return *this;
    }
};

template <typename T> inline bool operator==(Optional<T> const& lhs, Optional<T> const& rhs)
{
    // The optionals can't be equal if one has a value and the other does not.
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }

    if (!lhs.has_value()) {
        // Both optionals have no value.
        return true;
    }

    return *lhs == *rhs;
}

template <typename T> inline bool operator!=(Optional<T> const& lhs, Optional<T> const& rhs)
{
    return not(lhs == rhs);
}
