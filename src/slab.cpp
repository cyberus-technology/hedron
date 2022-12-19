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

void Slab::enqueue(Slab* new_prev, Slab* new_next)
{
    next = new_next;
    prev = new_prev;

    // To make sure that we don't screw up the list of slabs when we enqueue a new slab, we assert that
    // new_prev was the predecessor of new_next, and that new_next was the successor of new_prev.

    if (new_next != nullptr) {
        assert(new_next->prev == new_prev);
        new_next->prev = this;
    }

    if (new_prev != nullptr) {
        assert(new_prev->next == new_next);
        new_prev->next = this;
    }
}

void Slab::dequeue()
{
    if (prev != nullptr) {
        prev->next = next;
    }

    if (next != nullptr) {
        next->prev = prev;
    }
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
    slab->enqueue(nullptr, head);

    head = slab;
    curr = slab;
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

    // We can assert that head != nullptr here, because this can only happen if
    // someone calls free before calling alloc at least once, which is a bug.
    assert(head != nullptr);

    // The slab that holds the element that will be free'd. In the following comments it will be refered to as
    // 'this slab'.
    Slab* slab = reinterpret_cast<Slab*>(reinterpret_cast<mword>(ptr) & ~PAGE_MASK);

    const bool was_full = slab->full();

    slab->free(ptr); // Deallocate from slab

    // The list of slabs is ordered so that all full slabs come after curr, and all partial or free slabs
    // come before curr. We will reorder the list if necessary.

    if (EXPECT_FALSE(was_full)) {
        // This slab was full and is now partial. We will make curr point to it.

        if (slab->prev && slab->prev->full()) {
            // This slab's predecessor is full, thus we have to requeue it to make the above explained
            // invariant hold true.
            slab->dequeue();

            if (curr) {
                // curr is not a nullptr. We enqueue this slab between curr and curr's successor (which is a
                // full slab).
                slab->enqueue(curr, curr->next);
            } else {
                // curr is a nullptr, i.e. all slabs (except for this slab) are full. We enqueue this slab as
                // the new head.
                slab->enqueue(nullptr, head);
                head = slab;
            }
        }

        curr = slab;

    } else if (EXPECT_FALSE(slab->empty())) {
        // We want the slab cache to delete empty pages when a free leads to more than one empty slab in our
        // list of slabs. To ease checking for an empty slab, head always points to the empty slab in the list
        // if one exists.

        if (slab->prev == nullptr) {
            // This slab is now empty and has no prev, i.e. it is already the head. Thus we can just leave.
            assert(slab == head);
            return;
        }

        if (slab == curr) {
            // This slab shouldn't be curr, because it will be either deleted or moved to the head.
            assert(slab->prev != nullptr);
            curr = slab->prev;
        }

        slab->dequeue();

        if (slab->prev->empty() || head->empty()) {
            // There are already empty slabs, thus we delete this slab.
            assert(head != slab);
            delete slab;
        } else {
            // There is currently no empty slab, thus we enqueue this slab as the new head.
            slab->enqueue(nullptr, head);
            head = slab;
        }
    }
}
