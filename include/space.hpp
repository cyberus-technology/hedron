/*
 * Generic Space
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

#include "spinlock.hpp"

class Avl;
class Mdb;

class Space
{
    private:
        Spinlock    lock;
        Avl *       tree {nullptr};

    public:
        Mdb *tree_lookup (mword idx, bool next = false);

        static bool tree_insert (Mdb *node);
        static bool tree_remove (Mdb *node);

        void addreg (mword addr, size_t size, mword attr, mword type = 0);
        void delreg (mword addr);
};
