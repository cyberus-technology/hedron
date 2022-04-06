/*
 * Hedron Public API
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
 * Copyright (C) 2022 Sebastian Eydam, Cyberus Technology GmbH.
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

/// Hypercalls IDs
///
/// See chapter "Hypercall Numbers" in the Kernel Interface documentation.
enum class hypercall_id
{
    HC_CALL = 0,
    HC_REPLY = 1,
    HC_CREATE_PD = 2,
    HC_CREATE_EC = 3,
    HC_CREATE_SC = 4,
    HC_CREATE_PT = 5,
    HC_CREATE_SM = 6,
    HC_REVOKE = 7,
    HC_PD_CTRL = 8,
    HC_EC_CTRL = 9,
    HC_SC_CTRL = 10,
    HC_PT_CTRL = 11,
    HC_SM_CTRL = 12,
    HC_ASSIGN_PCI = 13,
    HC_ASSIGN_GSI = 14,
    HC_MACHINE_CTRL = 15,
    HC_CREATE_KP = 16,
    HC_KP_CTRL = 17,
};
