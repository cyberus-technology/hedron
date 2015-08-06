/*
 * Read-Copy Update (RCU)
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

#include "compiler.hpp"
#include "types.hpp"
#include "atomic.hpp"

class Rcu_elem
{
    public:
        Rcu_elem *next;
        void (*func)(Rcu_elem *);
        void (*pre_func)(Rcu_elem *);

        ALWAYS_INLINE
        explicit Rcu_elem (void (*f)(Rcu_elem *), void (*pf) (Rcu_elem *) = nullptr) : next (nullptr), func (f), pre_func(pf) {}
};

class Rcu_list
{
    public:
        Rcu_elem *  head;
        Rcu_elem ** tail;
        mword       count;

        ALWAYS_INLINE
        explicit Rcu_list() { clear(); }

        ALWAYS_INLINE
        inline void clear() { head = nullptr; tail = &head; count = 0;}

        ALWAYS_INLINE
        inline bool empty() { return &head == tail || head == nullptr; }

        ALWAYS_INLINE
        inline void append (Rcu_list *l)
        {
           *tail   = l->head;
            tail   = l->tail;
           *tail   = head;

            count += l->count;
            l->clear();
        }

        ALWAYS_INLINE
        inline bool enqueue (Rcu_elem *e)
        {
            Rcu_elem * const unused = nullptr;
            Rcu_elem * const in_use = reinterpret_cast<Rcu_elem *>(1);

            if ((e->next && e->next != in_use))
                /* double insertion in some queue */
                return false;

            if (e->next == in_use && tail != &e->next)
                /* element already in other than current queue */
                return false;

            if (!e->next)
                /* new element - mark as in use */
                if (!Atomic::cmp_swap (e->next, unused, in_use))
                    /* element got used by another queue */
                    return false;

            if (!Atomic::cmp_swap (*tail, *tail, e))
                /* element got enqueued by another queue */
                return false;

            count ++;

            tail = &e->next;

            return true;
        }
};

class Rcu
{
    private:
        static mword count;
        static mword state;

        static mword l_batch    CPULOCAL;
        static mword c_batch    CPULOCAL;

        static Rcu_list next    CPULOCAL;
        static Rcu_list curr    CPULOCAL;
        static Rcu_list done    CPULOCAL;

        enum State
        {
            RCU_CMP = 1UL << 0,
            RCU_PND = 1UL << 1,
        };

        ALWAYS_INLINE
        static inline mword batch() { return state >> 2; }

        ALWAYS_INLINE
        static inline bool complete (mword b) { return static_cast<signed long>((state & ~RCU_PND) - (b << 2)) > 0; }

        static void start_batch (State);
        static void invoke_batch();

    public:
        ALWAYS_INLINE
        static inline bool call (Rcu_elem *e) {
            if (e->pre_func)
                e->pre_func(e);

            return next.enqueue (e);
        }

        static void quiet();
        static void update();
};
