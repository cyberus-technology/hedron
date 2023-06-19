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

class Sm : public Typed_kobject<Kobject::Type::SM>, public Refcount, public Queue<Ec>
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

    Sm(Pd*, mword, mword = 0);
    ~Sm()
    {
        while (!counter)
            up(Ec::sys_finish<Sys_regs::BAD_CAP>);
    }

    inline void dn(bool zero, Ec* ec = Ec::current(), bool block = true)
    {
        {
            Lock_guard<Spinlock> guard(lock);

            if (counter) {
                counter = zero ? 0 : counter - 1;
                return;
            }

            if (!ec->add_ref()) {
                Sc::schedule(block);
                return;
            }

            enqueue(ec);
        }

        if (!block)
            Sc::schedule(false);

        ec->block_sc();
    }

    inline void up(void (*c)() = nullptr)
    {
        Ec* ec = nullptr;

        do {
            if (ec)
                Rcu::call(ec);

            {
                Lock_guard<Spinlock> guard(lock);

                if (!Queue<Ec>::dequeue(ec = Queue<Ec>::head())) {
                    counter++;
                    return;
                }
            }

            ec->release(c);

        } while (EXPECT_FALSE(ec->del_rcu()));
    }

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
