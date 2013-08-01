/*
 * Semaphore
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "ec.hpp"

class Sm : public Kobject, public Queue<Ec>
{
    private:
        mword counter;

        static Slab_cache cache;

        static void free (Rcu_elem * a) {
            Sm * sm = static_cast <Sm *>(a);

            while (!sm->counter)
                sm->up (Ec::sys_finish<Sys_regs::BAD_CAP, true>);

            delete sm;
        }

    public:
        Sm (Pd *, mword, mword = 0);
        ~Sm ()
        {
            while (!counter)
                up (Ec::sys_finish<Sys_regs::BAD_CAP, true>);
        }

        ALWAYS_INLINE
        inline void dn (bool zero, uint64 t)
        {
            Ec *ec = Ec::current;

            {   Lock_guard <Spinlock> guard (lock);

                if (counter) {
                    counter = zero ? 0 : counter - 1;
                    return;
                }

                if (!ec->add_ref()) {
                    Sc::schedule (true);
                    return;
                }

                enqueue (ec);
            }

            ec->set_timeout (t, this);

            ec->block_sc();
        }

        ALWAYS_INLINE
        inline void up(void (*c)() = Ec::sys_finish<Sys_regs::SUCCESS, true>)
        {
            Ec *ec = nullptr;

            do {
                if (ec)
                    Rcu::call (ec);

                {   Lock_guard <Spinlock> guard (lock);

                    if (!Queue<Ec>::dequeue (ec = Queue<Ec>::head())) {
                        counter++;
                        return;
                    }

                }

                ec->release (c);

            } while (EXPECT_FALSE(ec->del_rcu()));
        }

        ALWAYS_INLINE
        inline void timeout (Ec *ec)
        {
            {   Lock_guard <Spinlock> guard (lock);

                if (!dequeue (ec))
                    return;
            }

            ec->release (Ec::sys_finish<Sys_regs::COM_TIM>);
        }

        ALWAYS_INLINE
        static inline void *operator new (size_t) { return cache.alloc(); }

        ALWAYS_INLINE
        static inline void operator delete (void *ptr) { cache.free (ptr); }
};
