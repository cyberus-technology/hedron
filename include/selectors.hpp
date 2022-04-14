/*
 * Selectors
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#define SEL_NULL_DESC 0x0
#define SEL_KERN_CODE 0x8
#define SEL_KERN_DATA 0x10
#define SEL_USER_CODE 0x1b
#define SEL_USER_DATA 0x23
#define SEL_USER_CODE_L 0x2b
#define SEL_TSS_RUN 0x30
#define SEL_MAX 0x50
