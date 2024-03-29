/*
 * Hazards
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

inline constexpr unsigned HZD_SCHED{1u << 0};
inline constexpr unsigned HZD_RCU{1u << 1};
inline constexpr unsigned HZD_TLB{1u << 2}; // The TLB has to be flushed.
inline constexpr unsigned HZD_PRK{1u << 3}; // The CPU should be parked (call Lapic::park_function).
inline constexpr unsigned HZD_IDL{1u << 4}; // RCU acceleration.
inline constexpr unsigned HZD_RRQ{1u << 5}; // There are SCs in the ready queue and Sc::ready_enqueue has
                                            // to be called.
