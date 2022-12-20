/*
 * Utility Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "compiler.hpp"
#include "types.hpp"

template <typename T, T v> struct integral_constant {
    using value_type = T;

    static constexpr value_type value = v;
    constexpr operator value_type() const { return v; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

template <typename T> struct remove_reference {
    using type = T;
};
template <typename T> struct remove_reference<T&> {
    using type = T;
};
template <typename T> struct remove_reference<T&&> {
    using type = T;
};

template <typename T> struct is_lvalue_reference : false_type {
};
template <typename T> struct is_lvalue_reference<T&> : true_type {
};

template <typename T> constexpr T&& forward(typename remove_reference<T>::type& arg)
{
    return static_cast<T&&>(arg);
}

template <typename T> constexpr T&& forward(typename remove_reference<T>::type&& arg)
{
    static_assert(not is_lvalue_reference<T>::value, "Invalid rvalue to lvalue conversion");
    return static_cast<T&&>(arg);
}

template <typename T> typename remove_reference<T>::type&& move(T&& v)
{
    return static_cast<typename remove_reference<T>::type&&>(v);
}

template <typename T, typename U> struct is_same : false_type {
};

template <typename T> struct is_same<T, T> : true_type {
};

template <typename T> struct is_void : is_same<void, T> {
};

/// Wrap a member function together with its parameters into a callable.
///
/// Example:
///
/// std::for_each(begin, end, mem_fn_closure(&Foo:method)(param1, param2));
template <typename T, typename RET> auto mem_fn_closure(RET T::*method)
{
    return [method](auto... args) { return [method, args...](T& object) { (object.*method)(args...); }; };
}

#if !__STDC_HOSTED__

// Placement new operator
inline void* operator new(size_t, void* p) { return p; }

#endif
