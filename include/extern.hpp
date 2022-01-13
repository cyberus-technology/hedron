/*
 * External Symbols
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "memory.hpp"
#include "types.hpp"

extern char GIT_VER;

extern char PAGE_0[PAGE_SIZE];
extern char PAGE_1[PAGE_SIZE];
extern char PAGE_H[PAGE_SIZE];

extern char PDBRV;
extern char PDBR;

extern char LOAD_END;

extern mword FIXUP_S;
extern mword FIXUP_E;

extern void (*CTORS_C)();
extern void (*CTORS_G)();
extern void (*CTORS_E)();

extern int32 const PHYS_RELOCATION;

extern char entry_sysenter;
extern char entry_vmx;
extern mword handlers[];
extern mword hwdev_addr;

extern "C" char __start_all[];
extern "C" char __resume_bsp[];

extern "C" char __start_cpu[];
extern "C" char __start_cpu_end[];

extern "C" uint32 const __start_cpu_patch_jmp_dst;
extern "C" uint32 const __start_cpu_patch_rel[];
extern "C" uint32 const __start_cpu_patch_rel_end[];
