/*
 * Virtual LAPIC Page
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "vlapic.hpp"
#include "assert.hpp"
#include "buddy.hpp"

static_assert(sizeof(Vlapic) == PAGE_SIZE, "Virtual LAPIC page can only be page sized");

void* Vlapic::operator new(size_t size)
{
    assert(size == sizeof(Vlapic));

    return Buddy::allocator.alloc(0, Buddy::FILL_0);
}

void Vlapic::operator delete(void* ptr)
{
    mword const ptr_int{reinterpret_cast<mword>(ptr)};

    assert((ptr_int & PAGE_MASK) == 0);
    Buddy::allocator.free(reinterpret_cast<mword>(ptr));
}
