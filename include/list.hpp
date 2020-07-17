/*
 * List Element
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "compiler.hpp"
#include "types.hpp"

#if __STDC_HOSTED__
#include <iterator>
#endif

template <typename T>
class List_iterator
{
        T *ptr;

    public:

        using difference_type = ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

#if __STDC_HOSTED__
        using iterator_category = std::forward_iterator_tag;
#endif

        List_iterator(T *ptr_)
            : ptr {ptr_}
        {}

        T &operator*() const { return *ptr; }
        T *operator->() const { return ptr; }

        List_iterator &operator++()
        {
            ptr = ptr->next;
            return *this;
        }

        bool operator==(List_iterator<T> const &rhs) const { return ptr == rhs.ptr; }
        bool operator!=(List_iterator<T> const &rhs) const { return not (*this == rhs); }
};

template <typename T>
class List_range
{
        T *ptr;

    public:
        List_range(T *ptr_) : ptr{ptr_} {}

        List_iterator<T> begin() const { return {ptr}; }
        List_iterator<T> end()   const { return {nullptr}; }
};

template <typename T>
class List
{
    protected:
        friend class List_iterator<T>;

        T *next;

    public:
        explicit inline List (T *&list) : next (nullptr)
        {
            T **ptr;

            for (ptr = &list; *ptr; ptr = &(*ptr)->next) {
            }

            *ptr = static_cast<T *>(this);
        }
};
