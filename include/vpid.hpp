/*
 * Virtual Processor Identifier (VPID)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "compiler.hpp"

class Invvpid
{
private:
    uint64 vpid;
    uint64 addr;

public:
    inline Invvpid(unsigned long v, mword a) : vpid(v), addr(a) {}
};

class Vpid
{
public:
    enum Type
    {
        ADDRESS = 0,
        CONTEXT_GLOBAL = 1,
        CONTEXT_NOGLOBAL = 3
    };

    static inline void flush(Type t, unsigned long vpid, mword addr = 0)
    {
        Invvpid desc{vpid, addr};
        asm volatile("invvpid %0, %1" : : "m"(desc), "r"(static_cast<mword>(t)) : "cc");
    }
};
