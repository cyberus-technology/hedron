/*
 * A per-scope cleanup helper.
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

#include <util.hpp>

// Execute cleanup code before exiting from a scope.
//
// The cleanup function that is given on construction will be executed by the destructor of Scope_guard
// regardless how the scope is exited.
//
// Typical usage is:
//
// {
//     Scope_guard g([] { ... do some cleanup ... })};
//
//     if (something) {
//         // Cleanup happens here.
//         return;
//     }
//
//     // Cleanup happens here.
// }
//
// BEWARE: Scope_guard vs [[noreturn]]
//
// It is usually an error to use a Scope_guard in a function that calls [[noreturn]] functions. In this case,
// the destructor of Scope_guard is not called and will NOT call the cleanup function. A workaround is to
// introduce a new scope in which to use Scope_guard that does not contain calls to [[noreturn]] functions.
//
// Especially in the system call layer, Hedron uses a lot of [[noreturn]] functions and Scope_guard should be
// used with extra caution.
//
// BEWARE: Modifying state from Scope_guard cleanup functions.
//
// Consider the following example:
//
// int pitfall()
// {
//     int i {0};
//     Scope_guard g{[&i] { i = 1; }};
//
//     return i;
// }
//
// This function will return 0, not 1. This may be unexpected by developers. Conceptually, the return value is
// computed and _then_ the destructors run.
//
// Code that relies on this behavior should be avoided to keep code easy to understand and read.
//
// Please note that the cleanup function must be a function that returns void.
template <typename F> class Scope_guard
{
    F scope_cleanup_fn;

    static_assert(is_void<decltype(scope_cleanup_fn())>::value,
                  "Tried to instantiate a scope guard with a function that doesn't return void.");

public:
    Scope_guard() = delete;
    Scope_guard(Scope_guard&) = delete;
    Scope_guard& operator=(Scope_guard&) = delete;

    Scope_guard(F&& f) : scope_cleanup_fn(move(f)) {}
    Scope_guard(F const& f) : scope_cleanup_fn(f) {}

    ~Scope_guard() { scope_cleanup_fn(); }
};
