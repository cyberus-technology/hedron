/*
 * Selectors
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "config.hpp"

// The size of descriptors in the GDT.
#define DESC_SIZE 0x8

// The size of a TSS descriptor.
#define TSS_DESC_SIZE (2 * DESC_SIZE)

// Marks a segment selector for a userspace segment.
#define SEL_RPL3 0x3

// The constants below describe the GDT layout. All CPUs use the same GDT. From a high-level it looks like
// this:
//
// Selector        Global Descriptor Table (GDT)
//
//                 +-----------------------------------+
// SEL_NULL_DESC   | 0                                 |
// SEL_TSS_CPU0    | TSS Entry for CPU 0 (Low  32-bit) |
//                 | TSS Entry for CPU 0 (High 32-bit) |
//                 | TSS Entry for CPU 1 (Low  32-bit) |
//                 | TSS Entry for CPU 1 (High 32-bit) |
//                 |                                   |
//                 |   ... continues for each CPU ...  |
//                 |                                   |
// SEL_KERN_CODE   | Kernel Code Selector              |
// SEL_KERN_DATA   | Kernel Data Selector              |
// SEL_USER_CODE   | User Code Selector                |
// SEL_USER_DATA   | User Data Selector                |
// SEL_USER_CODE_L | User Code Selector (Long Mode)    |
//                 |                                   |
//                 |           ... unused ...          |
//                 |                                   |
// SEL_MAX         +-----------------------------------+

#define SEL_NULL_DESC 0x0

// We have one TSS for each CPU. They are consecutive in the GDT.
#define SEL_TSS_CPU0 DESC_SIZE

// We keep the code and data segments after the TSS descriptors. This allows us to limit the GDT limit to not
// include the user segments, in case we want to make iret to the user trap.
#define SEL_KERN_CODE (SEL_TSS_CPU0 + NUM_CPU * TSS_DESC_SIZE)
#define SEL_KERN_DATA (SEL_KERN_CODE + DESC_SIZE)
#define SEL_USER_CODE (SEL_KERN_CODE + 2 * DESC_SIZE + SEL_RPL3)
#define SEL_USER_DATA (SEL_KERN_CODE + 3 * DESC_SIZE + SEL_RPL3)
#define SEL_USER_CODE_L (SEL_KERN_CODE + 4 * DESC_SIZE + SEL_RPL3)

// Using this particular value is an optimization for the Intel VT exit handling. See the VM exit code in
// Vcpu::handle_vmx.
#define SEL_MAX 0x10000

#ifdef __cplusplus
static_assert(SEL_USER_CODE_L < SEL_MAX, "Too many CPUs to fit TSS descriptors into GDT");
#endif
