/*
 * Semaphore
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

#include "sm.hpp"
#include "stdio.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Sm::cache(sizeof(Sm), 32);

Sm::Sm(Pd* own, mword sel, mword cnt)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, Sm::PERM_ALL, free), counter(cnt)
{
    trace(TRACE_SYSCALL, "SM:%p created (CNT:%lu)", this, cnt);
}
