/*
 * Slab Allocator
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "buddy.hpp"
#include "initprio.hpp"

class Slab;

/**
 * The slab cache is an allocator for fixed size objects that are smaller than a page. The slab cache holds a
 * list of slabs. If the slab cache is full, i.e. all elements are allocated, it allocates a new, empty slab.
 */
class Slab_cache
{
private:
    Spinlock lock;

    // The slab that will be used for the next allocation, or a nullptr if the the slab cache is full.
    Slab* curr;
    Slab* head; // The head of our list of slabs.

    /*
     * Back end allocator
     */
    void grow();

public:
    unsigned long size; // Size of an element
    unsigned long buff; // Size of an element buffer (includes link field)
    unsigned long elem; // Number of elements that one slab can hold

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

/**
 * A slab is one page of memory that is split up into a number of fixed size elements. As long as an
 * element is not used, it holds a pointer to another unused element, i.e. this implementation uses a free
 * list to find unallocated elements.
 */
class Slab
{
public:
    unsigned long avail; // The amount of free elements in this slab
    Slab_cache* cache;
    Slab* prev; // Prev slab in cache
    Slab* next; // Next slab in cache
    char* head; // A pointer to the start of this slab's free list

    static inline void* operator new(size_t)
    {
        // The front-end allocator will initialize memory.
        return Buddy::allocator.alloc(0, Buddy::NOFILL);
    }

    static inline void operator delete(void* ptr) { Buddy::allocator.free(reinterpret_cast<mword>(ptr)); }

    Slab(Slab_cache* slab_cache);

    inline bool full() const { return !avail; }

    inline bool empty() const { return avail == cache->elem; }

    // Dequeues this slab. After dequeueing, this slab can't be found in the list of slabs anymore, but this
    // slab keeps its next and prev pointers.
    void dequeue();

    inline void* alloc();

    inline void free(void* ptr);
};
