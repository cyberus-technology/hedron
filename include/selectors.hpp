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

#define SEL_NULL_DESC 0x0
#define SEL_TSS_RUN 0x8
#define SEL_KERN_CODE 0x18
#define SEL_KERN_DATA 0x20
#define SEL_USER_CODE 0x2b
#define SEL_USER_DATA 0x33
#define SEL_USER_CODE_L 0x3b

// Using this particular value is an optimization for the Intel VT exit handling. See the VM exit code in
// Vcpu::handle_vmx.
#define SEL_MAX 0x10000
