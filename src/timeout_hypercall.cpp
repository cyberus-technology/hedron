/*
 * Hypercall Timeout
 *
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

#include "timeout_hypercall.hpp"
#include "sm.hpp"

void Timeout_hypercall::enqueue(uint64 t, Sm* s)
{
    if (sm && sm->del_rcu())
        Rcu::call(sm);

    if (!s->add_ref()) {
        sm = nullptr;
        return;
    }

    sm = s;
    Timeout::enqueue(t);
}

Timeout_hypercall::~Timeout_hypercall()
{
    if (sm && sm->del_rcu())
        Rcu::call(sm);
}

void Timeout_hypercall::trigger()
{
    if (sm)
        sm->timeout(ec);
}
