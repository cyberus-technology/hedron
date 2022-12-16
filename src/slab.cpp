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

#include "slab.hpp"
#include "assert.hpp"
#include "lock_guard.hpp"
#include "math.hpp"
#include "stdio.hpp"

Slab::Slab(Slab_cache* slab_cache)
    : avail(slab_cache->elem), cache(slab_cache), prev(nullptr), next(nullptr), head(nullptr)
{
    char* link = reinterpret_cast<char*>(this) + PAGE_SIZE - cache->buff + cache->size;

    for (unsigned long i = avail; i; i--, link -= cache->buff) {
        *reinterpret_cast<char**>(link) = head;
        head = link;
    }
}

void* Slab::alloc()
{
    avail--;

    void* link = reinterpret_cast<void*>(head - cache->size);
    head = *reinterpret_cast<char**>(head);
    return link;
}

void Slab::free(void* ptr)
{
    avail++;

    char* link = reinterpret_cast<char*>(ptr) + cache->size;
    *reinterpret_cast<char**>(link) = head;
    head = link;
}

Slab_cache::Slab_cache(unsigned long elem_size, unsigned elem_align)
    : curr(nullptr), head(nullptr), size(align_up(elem_size, sizeof(mword))),
      buff(align_up(size + sizeof(mword), elem_align)), elem((PAGE_SIZE - sizeof(Slab)) / buff)
{
    trace(TRACE_MEMORY, "Slab Cache:%p (S:%lu A:%u)", this, elem_size, elem_align);
}

void Slab_cache::grow()
{
    Slab* slab = new Slab(this);

    if (head)
        head->prev = slab;

    slab->next = head;
    head = curr = slab;
}

void* Slab_cache::alloc(Buddy::Fill fill_mem)
{
    void* ret;

    {
        Lock_guard<Spinlock> guard(lock);

        if (EXPECT_FALSE(!curr)) {
            grow();
        }

        assert(!curr->full());
        assert(!curr->next || curr->next->full());

        // Allocate from slab
        ret = curr->alloc();

        if (EXPECT_FALSE(curr->full())) {
            // curr always points to the slab that will be used for the next allocation. If curr is full, we
            // have to move it to curr-prev. If curr has no prev, curr will be a nullptr and the next
            // allocation will call Slab_cache::grow, which makes curr and head point to an empty slab.
            curr = curr->prev;
        }
    }

    Buddy::fill(ret, fill_mem, size);

    return ret;
}

void Slab_cache::free(void* ptr)
{
    Lock_guard<Spinlock> guard(lock);

    // we can assert that head != nullptr here, because this can only happen if
    // someone calls free before calling alloc at least once, which is a bug.
    assert(head != nullptr);

    // The slab that holds the element that will be free'd. In the following comments it will be refered to as
    // 'this slab'.
    Slab* slab = reinterpret_cast<Slab*>(reinterpret_cast<mword>(ptr) & ~PAGE_MASK);

    const bool was_full = slab->full();

    slab->free(ptr); // Deallocate from slab

    if (EXPECT_FALSE(was_full)) {

        // The list of slabs is ordered so that all full slabs come after curr, and all partial or free slabs
        // come before curr. Thus if this slab is in the 'full'-part of the list, it has to be requeued.
        if (slab->prev && slab->prev->full()) {

            // Dequeue
            slab->prev->next = slab->next;
            if (slab->next)
                slab->next->prev = slab->prev;

            // We want this slab to be the new curr.
            if (curr) {
                // curr is not a nullptr, i.e. the slab cache was not full. We enqueue this slab between a
                // full and a partial (or empty) slab.
                slab->prev = curr;
                slab->next = curr->next;
                curr->next = curr->next->prev = slab;
            }

            // Enqueue as head
            else {
                // curr is a nullptr, i.e. the last allocation lead to a completely full slab cache. We
                // enqueue this slab as the head of our list.
                slab->prev = nullptr;
                slab->next = head;
                head = head->prev = slab;
            }
        }

        // If this call to free lead to a partial slab that was full before, we always enqueue this slab
        // directly before a full slab. Thus this slab has to be the new curr.
        curr = slab;

    } else if (EXPECT_FALSE(slab->empty())) {

        // We want the slab cache to delete empty pages when a free leads to more than one empty slab in our
        // list of slabs. To ease checking for an empty slab, head always points to the empty slab in the list
        // if one exists.

        if (slab->prev) {

            if (slab == curr) {
                // This slab shouldn't be curr, because we will move it to the head.
                curr = slab->prev;
            }

            // This slab is empty but it is not the head. We either have to delete it or make it the new head.
            slab->prev->next = slab->next;
            if (slab->next)
                slab->next->prev = slab->prev;

            if (slab->prev->empty() || head->empty()) {
                // There are already empty slabs - delete current slab. We can assert that head != slab,
                // because we already know that this slab has a prev.
                assert(head != slab);
                delete slab;
            } else {
                // There are partial slabs in front of us and there is currently no empty slab, thus we can
                // enqueue this slab as the new head.
                slab->prev = nullptr;
                slab->next = head;
                head = head->prev = slab;
            }
        }
    }
}
