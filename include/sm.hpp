/*
 * Semaphore
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2014-2015 Alexander Boettcher, Genode Labs GmbH.
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

#include "ec.hpp"
#include "si.hpp"

class Sm : public Typed_kobject<Kobject::Type::SM>,
           public Refcount,
           public Queue<Ec>,
           public Queue<Si>,
           public Si
{
private:
    mword counter;

    static Slab_cache cache;

    static void free(Rcu_elem* a)
    {
        Sm* sm = static_cast<Sm*>(a);

        if (sm->del_ref())
            delete sm;
        else {
            sm->up();
        }
    }

public:
    // Capability permission bitmask.
    enum
    {
        PERM_UP = 1U << 0,
        PERM_DOWN = 1U << 1,

        PERM_ALL = PERM_UP | PERM_DOWN,
    };

    mword reset(bool l = false)
    {
        if (l)
            lock.lock();
        mword c = counter;
        counter = 0;
        if (l)
            lock.unlock();
        return c;
    }

    Sm(Pd*, mword, mword = 0, Sm* = nullptr, mword = 0);
    ~Sm()
    {
        while (!counter)
            up(Ec::sys_finish<Sys_regs::BAD_CAP, true>);
    }

    inline void dn(bool zero, uint64 t, Ec* ec = Ec::current(), bool block = true)
    {
        {
            Lock_guard<Spinlock> guard(lock);

            if (counter) {
                counter = zero ? 0 : counter - 1;

                Si* si;
                if (Queue<Si>::dequeue(si = Queue<Si>::head()))
                    ec->set_si_regs(si->value, static_cast<Sm*>(si)->reset());

                return;
            }

            if (!ec->add_ref()) {
                Sc::schedule(block);
                return;
            }

            Queue<Ec>::enqueue(ec);
        }

        if (!block)
            Sc::schedule(false);

        ec->set_timeout(t, this);

        ec->block_sc();

        ec->clr_timeout();
    }

    inline void up(void (*c)() = nullptr, Sm* si = nullptr)
    {
        Ec* ec = nullptr;

        do {
            if (ec)
                Rcu::call(ec);

            {
                Lock_guard<Spinlock> guard(lock);

                if (!Queue<Ec>::dequeue(ec = Queue<Ec>::head())) {

                    if (si) {
                        if (si->queued())
                            return;
                        Queue<Si>::enqueue(si);
                    }

                    counter++;
                    return;
                }
            }

            if (si)
                ec->set_si_regs(si->value, si->reset(true));

            ec->release(c);

        } while (EXPECT_FALSE(ec->del_rcu()));
    }

    inline void timeout(Ec* ec)
    {
        {
            Lock_guard<Spinlock> guard(lock);

            if (!Queue<Ec>::dequeue(ec))
                return;
        }

        ec->release(Ec::sys_finish<Sys_regs::COM_TIM>);
    }

    inline void add_to_rcu()
    {
        if (!add_ref())
            return;

        if (!Rcu::call(this))
            /* enqueued ? - drop our ref and add to rcu if necessary */
            if (del_rcu())
                Rcu::call(this);
    }

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
