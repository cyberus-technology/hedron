/*
 * Virtual-Memory Layout
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2018 Thomas Prescher, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

// Current virtual memory layout in the kernel:
//
// 0xffff_ffff_ffff_ffff END_SPACE_LIM - 1
// 0xffff_ffff_e000_0000 SPC_LOCAL_OBJ
// 0xffff_ffff_df00_0000 SPC_LOCAL_REMAP

// 0xffff_ffff_c000_2000 SPC_LOCAL_IOP_E
// 0xffff_ffff_c000_0000 SPC_LOCAL / SPC_LOCAL_IOP
// 0xffff_ffff_bfff_e000 TSS_AREA
// 0xffff_ffff_bfff_c000 CPU_LOCAL_APIC
// 0xffff_ffff_bfe0_0000 CPU_LOCAL
// 0xffff_ffff_bfdf_f000 HV_GLOBAL_FBUF

// 0xffff_ffff_8800_0000 LINK_ADDR

#define PAGE_BITS       12
#define PAGE_SIZE       (1 << PAGE_BITS)
#define PAGE_MASK       (PAGE_SIZE - 1)

#define LOAD_ADDR       0x0000000004000000

#define CANON_BOUND     0x0000800000000000
#define USER_ADDR       0x00007ffffffff000
#define LINK_ADDR       0xffffffff88000000
#define CPU_LOCAL       0xffffffffbfe00000
#define SPC_LOCAL       0xffffffffc0000000

#define HV_GLOBAL_FBUF  (CPU_LOCAL - PAGE_SIZE * 1)

#define CPU_LOCAL_APIC  (SPC_LOCAL - PAGE_SIZE * 4)

#define TSS_AREA        (SPC_LOCAL - PAGE_SIZE * 2)
#define TSS_AREA_E      (SPC_LOCAL)

#define SPC_LOCAL_IOP   (SPC_LOCAL)
#define SPC_LOCAL_IOP_E (SPC_LOCAL_IOP + PAGE_SIZE * 2)
#define SPC_LOCAL_REMAP (SPC_LOCAL_OBJ - 0x1000000)
#define SPC_LOCAL_OBJ   (END_SPACE_LIM - 0x20000000)

#define END_SPACE_LIM   (~0UL + 1)

// To boot APs, we need a piece of memory below 1MB to put the AP boot code.
#define APBOOT_ADDR     0x1000
