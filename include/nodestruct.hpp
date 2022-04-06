/*
 * Help class to avoid destruction of static variables
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "util.hpp"

template <typename T> class No_destruct
{
    alignas(T) char backing[sizeof(T)];

    // Prevent dynamic allocation
    void* operator new(size_t) noexcept { __builtin_trap(); }

public:
    template <typename... ARGS> No_destruct(ARGS&&... args) { new (backing) T(forward<ARGS>(args)...); }

    T* operator->() { return reinterpret_cast<T*>(backing); }
    T* operator->() const { return reinterpret_cast<T const*>(backing); }

    T* operator&() { return reinterpret_cast<T*>(backing); }
    T* operator&() const { return reinterpret_cast<T*>(backing); }
};
