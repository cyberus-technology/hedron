/*
 * Capability
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

#include "kobject.hpp"

class Capability
{
private:
    mword val;

    static mword const perm = 0x1f;

public:
    Capability() : val(0) {}

    Capability(Kobject* o, mword a) : val(a ? reinterpret_cast<mword>(o) | (a & perm) : 0) {}

    inline Kobject* obj() const { return reinterpret_cast<Kobject*>(val & ~perm); }

    inline unsigned prm() const { return val & perm; }
};

// Cast a capability to a specific Kobject type with dynamic type checking.
//
// The cast can perform additional permission bit checking if
// required_permissions is given. Returns nullptr in case the cast is invalid
// (just like dynamic_cast).
template <typename T> T* capability_cast(Capability const& cap, unsigned required_permissions = 0)
{
    Kobject* obj{cap.obj()};

    if (EXPECT_TRUE(obj and obj->type() == T::kobject_type and
                    (cap.prm() & required_permissions) == required_permissions)) {
        return static_cast<T*>(cap.obj());
    } else {
        return nullptr;
    }
}
