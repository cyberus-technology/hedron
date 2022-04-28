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

    T& operator*()
    {
        assert(has_value());
        return *data();
    }

    T const& operator*() const
    {
        assert(has_value());
        return *data();
    }

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
