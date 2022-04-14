/*
 * Budget Timeout
 *
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "timeout_budget.hpp"
#include "cpu.hpp"
#include "hazards.hpp"
#include "nodestruct.hpp"

// Including this field directly in Per_cpu creates a circular header
// dependency. So we have to compromise.
static No_destruct<Timeout_budget> percpu_budget[NUM_CPU];

void Timeout_budget::trigger() { Cpu::hazard() |= HZD_SCHED; }

void Timeout_budget::init() { Cpulocal::get().timeout_budget = &percpu_budget[Cpu::id()]; }
