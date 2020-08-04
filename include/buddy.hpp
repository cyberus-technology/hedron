/*
 * Buddy Allocator
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

#include "extern.hpp"
#include "memory.hpp"
#include "spinlock.hpp"

class Buddy
{
    private:
        class Block
        {
            public:
                Block *         prev;
                Block *         next;
                unsigned short  ord;
                unsigned short  tag;

                enum {
                    Used  = 0,
                    Free  = 1
                };
        };

        Spinlock        lock;
        signed long     max_idx;
        signed long     min_idx;
        mword           base;
        mword           order;
        Block *         index;
        Block *         head;

        inline signed long block_to_index (Block *b)
        {
            return b - index;
        }

        inline Block *index_to_block (signed long i)
        {
            return index + i;
        }

        inline signed long page_to_index (mword l_addr)
        {
            return l_addr / PAGE_SIZE - base / PAGE_SIZE;
        }

        inline mword index_to_page (signed long i)
        {
            return base + i * PAGE_SIZE;
        }

        inline mword virt_to_phys (mword virt)
        {
            return virt - reinterpret_cast<mword>(&OFFSET);
        }

        inline mword phys_to_virt (mword phys)
        {
            return phys + reinterpret_cast<mword>(&OFFSET);
        }

    public:
        enum Fill
        {
            NOFILL,
            FILL_0,
            FILL_1
        };

        static Buddy allocator;

        Buddy (mword phys, mword virt, mword f_addr, size_t size);

        static void fill(void *dst, Fill fill_mem, size_t size);

        void *alloc (unsigned short ord, Fill fill_mem);

        void free (mword addr);

        static inline void *phys_to_ptr (Paddr phys)
        {
            return reinterpret_cast<void *>(allocator.phys_to_virt (static_cast<mword>(phys)));
        }

        static inline mword ptr_to_phys (void *virt)
        {
            return allocator.virt_to_phys (reinterpret_cast<mword>(virt));
        }
};
