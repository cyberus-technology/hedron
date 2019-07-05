/*
 * Page Table Entry (PTE)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2015 Alexander Boettcher, Genode Labs GmbH
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
#include "buddy.hpp"
#include "x86.hpp"

template <typename P, typename E, unsigned L, unsigned B, bool F>
class Pte
{
    protected:
        E val;

        P *walk (E, unsigned long, bool = true);

        inline bool present() const { return val & P::PTE_P; }

        inline bool super() const { return val & P::PTE_S; }

        inline mword attr() const { return static_cast<mword>(val) & PAGE_MASK; }

        inline Paddr addr() const { return static_cast<Paddr>(val) & ~((1UL << order()) - 1); }

        inline mword order() const { return PAGE_BITS; }

        static inline mword order (mword) { return 0; }

        inline bool set (E o, E v)
        {
            bool b = Atomic::cmp_swap (val, o, v);

            if (F && b)
                clflush (this);

            return b;
        }

        static inline void *operator new (size_t)
        {
            void *p = Buddy::allocator.alloc (0, Buddy::FILL_0);

            if (F)
                clflush (p, PAGE_SIZE);

            return p;
        }

        static inline void operator delete (void *ptr) { Buddy::allocator.free (reinterpret_cast<mword>(ptr)); }

        void free_up (unsigned l, P *, mword, bool (*) (Paddr, mword, unsigned), bool (*) (unsigned, mword));

    public:

        Pte() : val(0) {}

        enum
        {
            ERR_P   = 1UL << 0,
            ERR_W   = 1UL << 1,
            ERR_U   = 1UL << 2,
        };

        enum Type
        {
            TYPE_UP,
            TYPE_DN,
        };

        // Returns the number of linear address bits that are used to index into the page table.
        static inline unsigned bits_per_level() { return B; }

        // Returns the number of levels in the page table.
        static inline unsigned max() { return L; }

        inline E root (mword l = L - 1) { return Buddy::ptr_to_phys (walk (0, l)); }

        size_t lookup (E, Paddr &, mword &);

        bool update (E, mword, E, mword, Type = TYPE_UP);

        void clear (bool (*) (Paddr, mword, unsigned) = nullptr, bool (*) (unsigned, mword) = nullptr);
};
