/*
 * Kernel Object
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
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

#include "mdb.hpp"
#include "refptr.hpp"

class Kobject : public Mdb
{
public:
    enum class Type : uint8
    {
        PD,
        EC,
        SC,
        PT,
        SM,
        KP,
        VCPU,
    };

    inline Type type() const { return objtype; }

private:
    Type const objtype;

protected:
    Spinlock lock;

    explicit Kobject(Type t, Space* s, mword b, mword a, void (*f)(Rcu_elem*), void (*pref)(Rcu_elem*))
        : Mdb(s, reinterpret_cast<mword>(this), b, a, f, pref), objtype(t)
    {
    }
};

template <Kobject::Type static_type> class Typed_kobject : public Kobject
{
public:
    // This member makes capability_cast work.
    static constexpr Type kobject_type{static_type};

    Typed_kobject(
        Space* s, mword b = 0, mword a = 0, void (*f)(Rcu_elem*) = [](Rcu_elem*) {},
        void (*pref)(Rcu_elem*) = nullptr)
        : Kobject(static_type, s, b, a, f, pref)
    {
    }
};
