/*
 * Memory allocation result type.
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

#include "result.hpp"

// An allocation failed because the allocator ran out of space.
struct Out_of_memory_error {
};

template <typename T> using Alloc_result = Result<T, Out_of_memory_error>;
using Alloc_result_void = Result_void<Out_of_memory_error>;
