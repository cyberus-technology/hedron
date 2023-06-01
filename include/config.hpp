/*
 * Configuration
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2022 Sebastian Eydam, Cyberus Technology GmbH.
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

/// Hedron API Version
///
/// This value needs to be increased for every incompatible API version by 1000
/// (major version bump). Backward compatible changes need to increase this
/// value by 1 (minor version bump).
///
/// For example, a change to the HIP or UTCB layouts or a change in hypercall
/// numbers is backwards incompatible and requires a major version bump. The
/// addition of a new hypercall without changing any of the existing hypercalls
/// is backwards compatible and requires a minor version bump.
///
/// Do not forget to update the CHANGELOG.md in the repository.
#define CFG_VER 12001

#define NUM_CPU 128
#define NUM_EXC 32
#define NUM_VMI 256

// The number of interrupt vectors that are programmable by userspace.
#define NUM_USER_VECTORS 192

#define NUM_LVT 6
#define NUM_IPI 4
#define NUM_PRIORITIES 128

#define SPN_SCH 0
#define SPN_HLP 1
#define SPN_RCU 2
#define SPN_VFI 4
#define SPN_VFL 5
#define SPN_LVT 7
#define SPN_IPI (SPN_LVT + NUM_LVT)
#define SPN_GSI (SPN_IPI + NUM_IPI + 1)

// This is the maximum number of IOAPICs possible, because they only have a 4-bit ID.
#define NUM_IOAPIC 16

// We have one stack per CPU. Each stack will have this size.
//
// One page of this will be sacrificed as a stack guard.
#define STACK_SIZE 0x3000
