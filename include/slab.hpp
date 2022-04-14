/*
 * Slab Allocator
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "buddy.hpp"
#include "initprio.hpp"

class Slab;

class Slab_cache
{
private:
    Spinlock lock;
    Slab* curr;
    Slab* head;

    /*
     * Back end allocator
     */
    void grow();

public:
    unsigned long size; // Size of an element
    unsigned long buff; // Size of an element buffer (includes link field)
    unsigned long elem; // Number of elements

    Slab_cache(unsigned long elem_size, unsigned elem_align);

    /*
     * Front end allocator
     */
    void* alloc(Buddy::Fill fill_mem = Buddy::FILL_0);

    /*
     * Front end deallocator
     */
    void free(void* ptr);
};

class Slab
{
public:
    unsigned long avail;
    Slab_cache* cache;
    Slab* prev; // Prev slab in cache
    Slab* next; // Next slab in cache
    char* head;

    static inline void* operator new(size_t)
    {
        // The front-end allocator will initialize memory.
        return Buddy::allocator.alloc(0, Buddy::NOFILL);
    }

    static inline void operator delete(void* ptr) { Buddy::allocator.free(reinterpret_cast<mword>(ptr)); }

    Slab(Slab_cache* slab_cache);

    inline bool full() const { return !avail; }

    inline bool empty() const { return avail == cache->elem; }

    void enqueue();
    void dequeue();

    inline void* alloc();

    inline void free(void* ptr);
};
