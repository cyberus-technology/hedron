/*
 * Read-Copy Update (RCU) lists
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "atomic.hpp"
#include "compiler.hpp"
#include "types.hpp"

class Rcu_elem
{
public:
    Rcu_elem* next;

    /// This callback is called when the object has no known references
    /// anymore and must reclaim the object.
    void (*func)(Rcu_elem*);

    /// This callback is called when the object is handed to Rcu::call().
    void (*pre_func)(Rcu_elem*);

    explicit Rcu_elem(void (*f)(Rcu_elem*), void (*pf)(Rcu_elem*) = nullptr)
        : next(nullptr), func(f), pre_func(pf)
    {
    }
};

class Rcu_list
{
public:
    Rcu_elem* head;
    Rcu_elem** tail;
    mword count;

    explicit Rcu_list() { clear(); }

    inline void clear()
    {
        head = nullptr;
        tail = &head;
        count = 0;
    }

    inline bool empty() { return &head == tail || head == nullptr; }

    inline void append(Rcu_list* l)
    {
        *tail = l->head;
        tail = l->tail;
        *tail = head;

        count += l->count;
        l->clear();
    }

    inline bool enqueue(Rcu_elem* e)
    {
        Rcu_elem* const unused = nullptr;
        Rcu_elem* const in_use = reinterpret_cast<Rcu_elem*>(1);

        if ((e->next && e->next != in_use))
            /* double insertion in some queue */
            return false;

        if (e->next == in_use && tail != &e->next)
            /* element already in other than current queue */
            return false;

        if (!e->next)
            /* new element - mark as in use */
            if (!Atomic::cmp_swap(e->next, unused, in_use))
                /* element got used by another queue */
                return false;

        if (!Atomic::cmp_swap(*tail, *tail, e))
            /* element got enqueued by another queue */
            return false;

        count++;

        tail = &e->next;

        return true;
    }
};
