/*
 * TLB cleanup and lazy page reclamation
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "buddy.hpp"
#include "compiler.hpp"
#include "types.hpp"
#include "util.hpp"

// Deferred cleanup of page table structures and TLB flush tracking.
//
// This class does not implement the TLB flushing logic itself as this is
// specific to the page table in question.
class Tlb_cleanup
{
    bool tlb_flush_{false};

public:
    using pointer = mword*;

    // Returns true, if a TLB flush is scheduled.
    WARN_UNUSED_RESULT bool need_tlb_flush() const { return tlb_flush_; }

    // Discard a scheduled TLB flush.
    //
    // This should be done with care as wrong usage will end up in TLB
    // invalidation bugs.
    void ignore_tlb_flush() { tlb_flush_ = false; }

    // Schedule a TLB flush.
    void flush_tlb_later() { tlb_flush_ = true; }

    // Free all pages that were marked for deferred reclamation immediately.
    void free_pages_now()
    {
        assert(not tlb_flush_);

        // Not implemented yet.
    }

    // Mark a page as to-be-freed after the next TLB flush.
    //
    // It is safe to be read from and written to until the TLB flush
    // actually happens.
    void free_later(pointer page)
    {
        tlb_flush_ = true;

        // This is not correct, because we need to defer freeing this page
        // until the TLB flush has happened. As the broken behavior was
        // already in the kernel, continue to live in the danger zone until
        // we fix this issue for good.
        Buddy::allocator.free(reinterpret_cast<mword>(page));
    }

    // Merge two Tlb_cleanup objects.
    //
    // This operation merges all deferred activity from the passed parameter
    // into the current instance. The parameter will become "empty" with no
    // deferred action pending.
    template <typename CLEANUP> void merge(CLEANUP&& rhs)
    {
        tlb_flush_ |= rhs.tlb_flush_;
        rhs.ignore_tlb_flush();
    }

    Tlb_cleanup& operator=(Tlb_cleanup&& rhs)
    {
        assert(not tlb_flush_);

        merge(rhs);
        return *this;
    }

    Tlb_cleanup(Tlb_cleanup&& rhs) { merge(rhs); }

    Tlb_cleanup& operator=(Tlb_cleanup const& rhs) = delete;
    Tlb_cleanup(Tlb_cleanup const& rhs) = delete;

    Tlb_cleanup() = default;
    explicit Tlb_cleanup(bool tlb_flush) : tlb_flush_{tlb_flush} {}

    // A named convenience constructor for readable code.
    static Tlb_cleanup tlb_flush(bool tlb_flush) { return Tlb_cleanup{tlb_flush}; }

    ~Tlb_cleanup()
    {
        // Once we fully implement this class, at destruction time there
        // should be no TLB flush pending and all pages can be freed.
        //
        // assert (not tlb_flush_);
    }
};
