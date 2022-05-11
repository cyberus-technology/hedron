/*
 * Host page table modification
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "generic_page_table.hpp"
#include "page_alloc_policy.hpp"
#include "page_table_policies.hpp"
#include "tlb_cleanup.hpp"

class Hpt;
using Hpt_page_table = Generic_page_table<9, mword, Atomic_access_policy<>, No_clflush_policy,
                                          Page_alloc_policy<>, Tlb_cleanup, Hpt>;

// Host Page Table
//
// Besides using this class to manage all normal CPU page tables, we also use it
// to store metainformation about memory type of each page (PTE_MT_MASK) and
// whether pages can be delegated (PTE_NODELEG).
class Hpt : public Hpt_page_table
{
private:
    // The number of leaf levels we support.
    static level_t supported_leaf_levels;

    static void flush()
    {
        mword cr3;
        asm volatile("mov %%cr3, %0; mov %0, %%cr3" : "=&r"(cr3));
    }

    /// Invalidate a single page in the current address space.
    static void flush_one_page(void* page)
    {
        // We add a memory clobber, because it is usually not desirable to reorder memory accesses beyond page
        // flushes.
        //
        // Note that invlpg takes a memory operand, but does not read what's in this memory operand. The page
        // that contains the address of the memory operand is used for TLB invalidation.
        asm volatile("invlpg (%0)" ::"r"(page) : "memory");
    }

    // Returns true, if the page table is currently active.
    bool is_active() const
    {
        mword cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));

        return (cr3 & ~PAGE_MASK) == root();
    }

    using Hpt_page_table::Hpt_page_table;

public:
    enum : mword
    {
        // The bitmask covers legal memory type values as we get them from
        // the MTRRs.
        MT_MASK = 0b111UL,
    };

    enum : ord_t
    {
        PTE_MT_SHIFT = 53,
    };

    enum : pte_t
    {
        PTE_P = 1ULL << 0,
        PTE_W = 1ULL << 1,
        PTE_U = 1ULL << 2,
        PTE_UC = 1ULL << 4,
        PTE_S = 1ULL << 7,
        PTE_G = 1ULL << 8,

        PTE_A = 1ULL << 5,
        PTE_D = 1ULL << 6,

        // We encode memory types in available bits.
        PTE_MT_MASK = MT_MASK << PTE_MT_SHIFT,

        PTE_PAT0 = 1ULL << 3,
        PTE_PAT1 = 1ULL << 4,
        PTE_PAT2 = 1ULL << 7, // Only valid in leaf level (otherwise it's bit 12)

        // Prevent pages from being delegated. This is useful for pages that
        // the kernel needs to be able to reclaim from userspace (e.g. UTCBs,
        // vLAPIC pages).
        PTE_NODELEG = 1ULL << 56,

        PTE_NX = 1ULL << 63,
    };

    enum : uint32
    {
        ERR_W = 1U << 1,
        ERR_U = 1U << 2,
    };

    static constexpr pte_t all_rights{PTE_P | PTE_W | PTE_U | PTE_A | PTE_D};
    static constexpr pte_t mask{PTE_NX | PTE_MT_MASK | PTE_NODELEG | PTE_UC | PTE_G | all_rights};

    // Adjust the number of leaf levels to the given value.
    static void set_supported_leaf_levels(level_t level);

    // Return a structural copy of this page table for the given virtual
    // address range.
    Hpt deep_copy(mword vaddr_start, mword vaddr_end);

    void make_current(mword pcid)
    {
        mword phys_root{root()};

        assert(leaf_levels() <= supported_leaf_levels);
        assert((phys_root & PAGE_MASK) == 0);

        asm volatile("mov %0, %%cr3" : : "r"(phys_root | pcid) : "memory");
    }

    // The limit of how much memory can be accessed safely after remap().
    static const size_t remap_guaranteed_size;

    // Temporarily map the given physical memory.
    //
    // Establish a temporary mapping for the given physical address in a
    // special kernel virtual address region reserved for this
    // usecase. Return a pointer to access this memory. phys does not need
    // to be aligned.
    //
    // If use_boot_hpt is true, the mapping is established in the boot page
    // tables. If not, use the current Pd's kernel address space.
    //
    // The returned pointer is valid until the next remap call (on any core).
    static void* remap(Paddr phys, bool use_boot_hpt = true);

    /// Unmap a page from the kernel address space.
    ///
    /// This function only allows to modify boot_hpt to keep the kernel address space identical everywhere.
    /// The kernel portion of the address space is replicated from the boot_hpt into all other host page
    /// tables. If we allow modifying other page tables beyond boot_hpt, we risk a non-uniform kernel address
    /// space.
    ///
    /// This function also demands that boot_hpt is currently active. Because the boot_hpt is copied into
    /// newly created address spaces, we have to make sure to only call it before new address spaces are
    /// created. This time frame largely coincides with the time the boot_hpt is active.
    static void unmap_kernel_page(void* kernel_page);

    // Atomically change a 4K page mapping to point to a new frame. Return
    // the physical address that backs vaddr.
    Paddr replace(mword vaddr, mword paddr);

    // Create a page table from existing page table structures.
    explicit Hpt(pte_pointer_t rootp) : Hpt_page_table(4, supported_leaf_levels, rootp) {}

    // Create a page table from scratch.
    Hpt() : Hpt_page_table(4, supported_leaf_levels) {}

    // Create the page table that is (ab)used as physical memory
    // database. This is never actually used as a page table by the CPU, so
    // we can pretend to have really large pages to save valuable kernel
    // memory.
    static Hpt make_golden_hpt() { return Hpt(4, 4); }

    // Convert mapping database attributes to page table attributes.
    static pte_t hw_attr(mword a);

    // Return the intersection of rights from source and
    // desired. Metainformation (memory type) is carried forward from
    // source.
    static pte_t merge_hw_attr(pte_t source, pte_t desired);

    // Return the PAT memory type of a HPT page table entry.
    static mword attr_to_pat(pte_t source)
    {
        return ((source & PTE_PAT0) ? 1U << 0 : 0) | ((source & PTE_PAT1) ? 1U << 1 : 0) |
               ((source & PTE_PAT2) ? 1U << 2 : 0);
    }

    // The boot page table as constructed in start.S.
    static Hpt& boot_hpt();
};
