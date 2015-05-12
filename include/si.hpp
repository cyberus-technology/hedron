/*
 * Signal
 *
 * Copyright (C) 2014-2015 Alexander Boettcher, Genode Labs GmbH.
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

#include "types.hpp"
#include "queue.hpp"

class Sm;

class Si
{
    friend class Queue<Si>;

    private:
        Sm *        sm;
        Si *        prev;
        Si *        next;

    public:
        mword const value;

        Si (Sm *, mword);
        ~Si();

        ALWAYS_INLINE
        inline bool is_signal() const { return sm; }

        ALWAYS_INLINE
        inline bool queued() const { return next; }

        void chain(Sm *s);

        void submit();
};
