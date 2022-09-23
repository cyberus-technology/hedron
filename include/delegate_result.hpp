/*
 * Delegation result and error type.
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

#include "alloc_result.hpp"
#include "result.hpp"

// A delegation failed. This can happen for different reasons. See the different constructors below.
struct Delegate_error {
    enum class type
    {
        OUT_OF_MEMORY,
        INVALID_MAPPING,
    };

    type error_type;

    Delegate_error() = delete;

    // These constructors should never be called manually. Functions that fail to allocate memory return
    // Out_of_memory_error and these constructors will automatically convert this to Delegate_error as needed.
    Delegate_error(Out_of_memory_error const&) : error_type(type::OUT_OF_MEMORY) {}
    Delegate_error(Out_of_memory_error&&) : error_type(type::OUT_OF_MEMORY) {}

    Delegate_error(type error_type_) : error_type(error_type_) {}

    // A delegation failed because the source or destination addresses are invalid.
    static Delegate_error invalid_mapping() { return type::INVALID_MAPPING; }
};

template <typename T> using Delegate_result = Result<T, Delegate_error>;
using Delegate_result_void = Result_void<Delegate_error>;
