/*
 * Signal
 *
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

#include "queue.hpp"
#include "types.hpp"

class Sm;

class Si
{
    friend class Queue<Si>;

private:
    Sm* sm;
    Si* prev;
    Si* next;

public:
    mword const value;

    Si(Sm*, mword);
    ~Si();

    inline bool is_signal() const { return sm; }

    inline bool queued() const { return next; }

    void submit();
};
