/*
 * A Rust-like Result type.
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
#include "monostate.hpp"
#include "util.hpp"

// A wrapper around error values.
//
// This type autoconverts into the appropriate Result type below. Typical usage in a function that returns
// Result<T, E> would be:
//
// return Err(SomeE);
template <typename E> struct [[nodiscard]] Err {
    E value;

    Err(E&& err) : value(move(err)) {}
    Err(E const& err) : value(err) {}
};

// A wrapper around success values.
//
// This type autoconverts into the appropriate Result type below. Typical usage in a function that returns
// Result<T, E> would be:
//
// return Ok(SomeT);
template <typename T> struct [[nodiscard]] Ok {
    T value;

    Ok(T&& ok) : value(move(ok)) {}
    Ok(T const& ok) : value(ok) {}
};

// A matching Ok type alias for Result_void.
//
// This alias avoids having to manually specify the otherwise mostly invisible monostate type.
using Ok_void = Ok<monostate>;

// A Rust-like result type.
//
// Proper error handling is a cornerstone of robust software. Unfortunately, the C++ community has not
// embraced a single error handling mechanism. Besides exceptions, which are not usable in the Hedron context,
// there is a variety of error handling styles.
//
// For Hedron, we embrace Rust-style error handling using the Result type. Result is a container that is
// conceptually similar to std::variant. Result carries a result of a computation of type T or an error value
// of type E.
//
// Useful error types are enum class or simple structs containing an enum class. It is useful to implement
// type conversion operators between error types to unclutter error handling. See the TRY_OR_RETURN macro,
// which is the equivalent of the Rust '?' operator.
//
// To extract the result of a computation, it is recommended to use the monadic operators, such as
// and_then. This avoids unwrap operations, which can panic, if used incorrectly.
//
// If explicit unwrap operations in code are necessary, they should be accompanied by an explanation why it
// cannot panic.
//
// This type is marked as nodiscard to prevent errors from being ignored.
template <typename T, typename E> class [[nodiscard]] Result
{
    union {
        T ok_value;
        E err_value;
    };

    bool has_ok;

public:
    using ok_t = T;
    using err_t = E;

    Result(Result const& r) : has_ok(r.has_ok)
    {
        if (has_ok) {
            new (&ok_value) ok_t(r.ok_value);
        } else {
            new (&err_value) err_t(r.err_value);
        }
    }

    Result(Result&& r) : has_ok(r.has_ok)
    {
        if (has_ok) {
            new (&ok_value) ok_t(move(r.ok_value));
        } else {
            new (&err_value) err_t(move(r.err_value));
        }
    }

    template <typename OK> Result(Ok<OK> const& ok) : ok_value(ok.value), has_ok(true) {}
    template <typename OK> Result(Ok<OK>&& ok) : ok_value(move(ok.value)), has_ok(true) {}

    template <typename ERR> Result(Err<ERR> const& err) : err_value(err.value), has_ok(false) {}
    template <typename ERR> Result(Err<ERR>&& err) : err_value(move(err.value)), has_ok(false) {}

    Result& operator=(Result const&) = delete;
    Result& operator=(Result&&) = delete;

    ~Result()
    {
        if (has_ok) {
            ok_value.~ok_t();
        } else {
            err_value.~err_t();
        }
    }

    // Construct a Result for a successful computation.
    static Result ok(ok_t const& t) { return Ok(t); }
    static Result ok(ok_t&& t) { return Ok(move(t)); }

    // Construct a Result for a failed computation.
    static Result err(err_t const& e) { return Err(e); }
    static Result err(err_t&& e) { return Err(move(e)); }

    bool is_ok() const { return has_ok; }
    bool is_err() const { return not has_ok; }

    // Unwrap the contained OK value.
    //
    // It is a bug to call this function when the result does not contain an OK value.
    T unwrap() const
    {
        assert(is_ok());
        return ok_value;
    }

    // Unwrap the contained OK value. If it doesn't exist, compute a value using the given function.
    template <typename F> T unwrap_or_else(F&& f) const
    {
        if (is_ok()) {
            return ok_value;
        } else {
            return f();
        }
    }

    // Unwrap the contained error value.
    //
    // It is a bug to call this function when the result does not contain an error value.
    E unwrap_err() const
    {
        assert(is_err());
        return err_value;
    }

    // If the result contains an OK value, map it using the given function.
    template <typename F> auto map(F&& f) const -> Result<decltype(f(ok_value)), err_t>
    {
        if (is_ok()) {
            return Ok(f(ok_value));
        } else {
            return Err(err_value);
        }
    }

    // If the result contains an error value, map it using the given function.
    template <typename F> auto map_err(F&& f) const -> Result<ok_t, decltype(f(err_value))>
    {
        if (is_ok()) {
            return Ok(ok_value);
        } else {
            return Err(f(err_value));
        }
    }

    // If the result holds an OK value, hand it to the given function.
    template <typename F> auto and_then(F&& f) const -> Result<typename decltype(f(ok_value))::ok_t, err_t>
    {
        if (is_ok()) {
            return f(unwrap());
        } else {
            return Err(unwrap_err());
        }
    }
};

// A return type for functions that want to return void, but can also fail.
//
// See also Ok_void above.
template <typename E> using Result_void = Result<monostate, E>;

// Try a fallible operation and return from the current function, if it fails. Otherwise, this is an
// expression that evaluates to the unwrapped OK value.
//
// In short, this is a poor people's version of the Rust question mark operator.
//
// To use this macro, the current function must return a Result. The given expression must evaluate to a
// Result with an error type that is convertible to the error type of the function return value.
//
// Consider a function example that returns a Result<int, Error>. TRY_OR_RETURN can be used like this:
//
// Result<int, Error> increment_example()
// {
//     // If example() returns an error, we return it from this function as well.
//     int i {TRY_OR_RETURN(example())};
//
//     // Otherwise, we continue with the return value.
//     return Ok(i + 1);
// }
#define TRY_OR_RETURN(expr)                                                                                  \
    ({                                                                                                       \
        auto const result__{expr};                                                                           \
        if (result__.is_err())                                                                               \
            return Err(result__.unwrap_err());                                                               \
        result__.unwrap();                                                                                   \
    })
