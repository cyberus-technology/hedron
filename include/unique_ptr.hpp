/*
 * A simple smart pointer implementation for unique pointers
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "util.hpp"

// A simplified unique_ptr implementation.
//
// This class works exactly like std::unique_ptr, but there is no support for
// custom deleters and type conversions are not supported yet.
template <typename T>
class Unique_ptr
{
    public:
        Unique_ptr(T *ptr_ = nullptr) : ptr (ptr_) {}
        Unique_ptr(Unique_ptr &&ref)   : ptr (ref.release()) {}

        Unique_ptr(Unique_ptr const & ref) = delete;

        T *operator->() const { return  ptr; }
        T &operator* () const { return *ptr; }

        Unique_ptr &operator=(Unique_ptr &&ref)
        {
            reset(ref.release());
            return *this;
        }

        Unique_ptr &operator=(Unique_ptr const &ref) = delete;

        ~Unique_ptr() { reset(); }

        T *get() const { return ptr; }

        operator bool() const
        {
            return ptr != nullptr;
        }

        // Release ownership of the object.
        T *release()
        {
            T *old {ptr};

            ptr = nullptr;
            return old;
        }

        // Reset the stored pointer to the given pointer.
        void reset(T *new_ptr = nullptr)
        {
            T *released {release()};

            // Delete should accept nullptr, but the buddy allocator doesn't
            // like it and changing it to accept nullptr might mask bugs.
            if (released) {
                delete released;
            }

            ptr = new_ptr;
        }

    private:

        // The underlying pointer.
        T *ptr {nullptr};
};

template <typename T, typename... ARGS>
inline Unique_ptr<T> make_unique(ARGS&&... args)
{
    return { new T (forward<ARGS>(args)...) };
}
