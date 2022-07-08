/*
 * Standard I/O
 *
 * Copyright (C) 2022 Julian Stecklina, Cyberus Technology
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

#include "stdio.hpp"
#include "cpu.hpp"
#include "cpulocal.hpp"

// This trivial function does not live in the header to avoid pulling in many headers from trace.hpp.
int trace_id() { return Cpulocal::is_initialized() ? Cpu::id() : -1; }
