/*
 * Generic page table modification
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "compiler.hpp"
#include "memory.hpp"
#include "types.hpp"

// Generic page table modification
//
// This class implements generic page table modification with the following
// features:
//
// - compile-time configurable entry types, attributes, and memory access
// - run-time configurable page table levels (useful for Intel IOMMUs)
// - atomic page table updates on PAGE_SIZE granularity
//
// Access to memory is handled via the MEMORY class template parameter. Memory
// reclamation is handled via PAGE_ALLOC. Pages that might still be referenced
// by other cores until the next TLB flush are collected by DEFERRED_CLEANUP and
// have to be handled by the initiator of page table modifications.
//
// For an example of how to implement these dependencies, check the unit tests.
//
// The page table modification is concurrency-safe, if the memory operations
// provided by MEMORY are atomic and sequentially-consistent. If that is true,
// all public interfaces can be safely used without synchronization. The
// semantics for two concurrent updates to the page table are thus that in the
// overlapping region, the updates may be arbitrarily interleaved.
//
template <int BITS_PER_LEVEL, typename ENTRY, typename MEMORY,
          typename PAGE_ALLOC, typename DEFERRED_CLEANUP, typename ATTR>
class Generic_page_table
{
        using this_t = Generic_page_table<BITS_PER_LEVEL, ENTRY, MEMORY, PAGE_ALLOC,
                                          DEFERRED_CLEANUP, ATTR>;

    public:
        using level_t = int;
        using ord_t = int;

        using virt_t = ENTRY;
        using phys_t = ENTRY;

        using pte_t = ENTRY;
        using pte_pointer_t = typename MEMORY::pointer;

        struct Mapping
        {
            public:
                virt_t vaddr {0};
                phys_t paddr {0};

                // No attributes -> empty mapping.
                pte_t attr  {0};
                ord_t order {0};

                bool present() const { return attr & ATTR::PTE_P; }

                // Returns the size of the mapping in bytes.
                size_t size() const { return static_cast<size_t>(1) << order; }

                bool operator==(Mapping const &rhs) const
                {
                    return vaddr == rhs.vaddr and paddr == rhs.paddr and attr == rhs.attr and order == rhs.order;
                }
        };

    private:

        MEMORY     memory_;
        PAGE_ALLOC page_alloc_;

        // Valid page table levels range from 0 to max_levels_ - 1.
        level_t const max_levels_;

        // Indicates on which levels translation can stop.
        //
        // Page table levels that can be leaves are 0 to leaf_levels_ - 1. For
        // example, on x86_64 with 1 GB pages, levels 0 (4K), 1 (2M), and 2
        // (1GB) can be leaves, so this member would be set to 3.
        level_t const leaf_levels_;

        // The root of the page table hierarchy.
        pte_pointer_t root_;

        // The maximum possible mapping order.
        ord_t max_order() const { return max_levels_ * BITS_PER_LEVEL + PAGE_BITS; }

        // Return the order that an entry at a specific page table level has.
        ord_t level_order(level_t level) const { return level * BITS_PER_LEVEL + PAGE_BITS; }

        // Returns the index into the page table for the given level.
        size_t virt_to_index(level_t level, virt_t vaddr) const
        {
            return (vaddr >> level_order(level)) & ((static_cast<ENTRY>(1) << BITS_PER_LEVEL) - 1);
        }

        bool is_superpage(level_t level, pte_t entry) const
        {
            assert (not ((level == 0 or level >= leaf_levels_) and (entry & ATTR::PTE_S)));
            return (entry & ATTR::PTE_P) and level < leaf_levels_ and (entry & ATTR::PTE_S);
        }

        bool is_leaf(level_t level, pte_t entry) const
        {
            return level == 0 or not (entry & ATTR::PTE_P) or is_superpage (level, entry);
        }

        Mapping lookup(virt_t vaddr, pte_pointer_t pte_p, level_t cur_level)
        {
            assert (cur_level >= 0 and cur_level < max_levels_);

            pte_t  const entry {memory_.read (pte_p + virt_to_index (cur_level, vaddr))};
            phys_t const phys  {entry & ~ATTR::mask};

            if (is_leaf (cur_level, entry)) {
                ord_t const map_order {level_order(cur_level)};
                ENTRY const mask {(static_cast<ENTRY>(1) << map_order) - 1};

                return Mapping {vaddr & ~mask, phys & ~mask, entry & ATTR::mask, map_order};
            }

            return lookup (vaddr, page_alloc_.phys_to_pointer (phys), cur_level - 1);
        }

        // Use a superpage from the given level to fill out a new page table one
        // hierarchy deeper with the same mappings.
        void fill_from_superpage(pte_pointer_t new_table, pte_t superpage_pte, level_t cur_level)
        {
            assert (is_superpage(cur_level, superpage_pte));

            pte_t attr_mask {cur_level == 1 ? static_cast<pte_t>(ATTR::PTE_S) : 0};

            for (size_t i {0}; i < static_cast<size_t>(1) << BITS_PER_LEVEL; i++) {
                pte_t offset {i << (PAGE_BITS + (cur_level - 1) * BITS_PER_LEVEL)};

                memory_.write (new_table + i, (superpage_pte & ~attr_mask) | offset);
            }
        }

        // See the description of the public version of this function below.
        pte_pointer_t walk_down_and_split(DEFERRED_CLEANUP &cleanup, virt_t vaddr, level_t to_level, pte_pointer_t pte_p,
                                          level_t cur_level, bool create)
        {
            assert (cur_level >= 0 and cur_level <  max_levels_);
            assert (to_level  >= 0 and to_level  <= cur_level);

            if (to_level == cur_level) {
                return pte_p;
            }

        retry:

            auto   entry_p {pte_p + virt_to_index (cur_level, vaddr)};
            pte_t  entry   {memory_.read (entry_p)};
            phys_t phys    {entry & ~ATTR::mask};

            assert (cur_level != 0);

            // In case there is no mapping and we want to downgrade rights, we
            // can already stop.
            if (not (entry & ATTR::PTE_P) and not create) {
                return nullptr;
            }

            // We have hit a leaf entry, but need to recurse further. Create the
            // next page table level.
            if (not (entry & ATTR::PTE_P) or is_superpage (cur_level, entry)) {
                auto   const new_page  {page_alloc_.alloc_zeroed_page()};
                phys_t const new_phys  {page_alloc_.pointer_to_phys (new_page)};
                pte_t  const new_entry {new_phys | (ATTR::all_rights & ~ATTR::PTE_S)};

                // Initialize the new page table with content from the former
                // superpage.
                if (is_superpage (cur_level, entry)) {
                    fill_from_superpage (new_page, entry, cur_level);
                    cleanup.flush_tlb_later();
                }

                // If we fail to install a pointer to the new page, we can
                // reclaim it immediately, because no other CPU holds a
                // reference.
                if (not memory_.compare_exchange (entry_p, entry, new_entry)) {
                    page_alloc_.free_page (new_page);
                    goto retry;
                }

                entry = new_entry;
                phys  = new_phys;
            }

            assert (not is_leaf (cur_level, entry));
            return walk_down_and_split (cleanup, vaddr, to_level, page_alloc_.phys_to_pointer (phys),
                                        cur_level - 1, create);
        }

        // Free any page tables referenced from a page table entry.
        //
        // Assumes that the given page table entry is already removed from the
        // page table.
        void cleanup(DEFERRED_CLEANUP &cleanup_state, pte_t pte, level_t cur_level)
        {
            assert (cur_level >= 0 and cur_level < max_levels_);

            if (is_leaf (cur_level, pte)) {
                if (pte & ATTR::PTE_P) {
                    cleanup_state.flush_tlb_later();
                }
            } else {
                cleanup_table(cleanup_state, page_alloc_.phys_to_pointer (pte & ~ATTR::mask),
                              cur_level);
            }
        }

        // Free any page tables referenced from the given page table including
        // itself. Companion function to cleanup().
        void cleanup_table(DEFERRED_CLEANUP &cleanup_state, pte_pointer_t table, level_t cur_level)
        {
            assert (cur_level > 0 and cur_level <= max_levels_);

            for (size_t i {0}; i < static_cast<size_t>(1) << BITS_PER_LEVEL; i++) {
                cleanup(cleanup_state, memory_.read (table + i), cur_level - 1);
            }

            cleanup_state.free_later (table);
        }

        // Recursively update page table structures with new mappings.
        void fill_entries(DEFERRED_CLEANUP &cleanup_state, pte_pointer_t table, level_t cur_level,
                          Mapping const &map)
        {
            assert (table != nullptr);
            assert (cur_level >= 0 and cur_level < max_levels_);

            ord_t const entry_order {level_order(cur_level)};
            ord_t const table_order {level_order(cur_level + 1)};

            assert (map.order >= entry_order and map.order <= table_order);

            // The order of how many entries we need to update in this level.
            ord_t const updated_order {map.order - entry_order};

            // The offset at which we start in the current page table.
            size_t const offset {virt_to_index(cur_level, map.vaddr)};

            // The current level allows creating superpages.
            bool const create_superpages {cur_level > 0 and cur_level < leaf_levels_};

            // Track special case where we only want to unmap memory.
            bool const clear_mappings {not map.present()};

            // We've hit the page table leafs.
            bool const is_leaf {cur_level == 0 or create_superpages or clear_mappings};

            for (size_t i = 0; i < (static_cast<size_t>(1) << updated_order); i++) {
                ENTRY const addr_offset {static_cast<ENTRY>(i) << entry_order};
                pte_pointer_t const pte_p {table + i + offset};

                if (is_leaf) {
                    pte_t const new_attr {map.attr | (create_superpages ? static_cast<pte_t>(ATTR::PTE_S) : 0)};
                    pte_t const new_pte {clear_mappings ? 0 : (map.paddr | addr_offset | new_attr)};

                    cleanup (cleanup_state, memory_.exchange (pte_p, new_pte), cur_level);
                } else {
                retry:

                    pte_t old_pte {memory_.read (pte_p)};

                    // We have to create entries at a lower level, but there is
                    // no page table yet.
                    if (not (old_pte & ATTR::PTE_P)) {

                        auto  const zero_page {page_alloc_.alloc_zeroed_page()};
                        pte_t const new_pte {page_alloc_.pointer_to_phys (zero_page) | ATTR::all_rights};

                        if (not memory_.compare_exchange (pte_p, old_pte, new_pte)) {
                            page_alloc_.free_page (zero_page);
                            goto retry;
                        }

                        cleanup (cleanup_state, old_pte, cur_level);
                        old_pte = new_pte;
                    }

                    Mapping const sub_map {map.vaddr + addr_offset, map.paddr + addr_offset,
                            map.attr, entry_order};

                    fill_entries (cleanup_state, page_alloc_.phys_to_pointer (old_pte & ~ATTR::mask),
                                  cur_level - 1, sub_map);
                }
            }
        }

    public:

        // Return the memory abstraction as a unit testing aid.
        MEMORY const &memory() const { return memory_; }

        // Return the page_alloc abstraction as a unit testing aid.
        PAGE_ALLOC const &page_alloc() const { return page_alloc_; }

        // Returns the total number of levels this page table has.
        level_t max_levels() const { return max_levels_; }

        // Returns the number of page table levels that can have leaf
        // entries. For a 4-level page table that only supports 2MB pages, this
        // would return 2, because only lowest two levels support entries that
        // terminate the page walk.
        level_t leaf_levels() const { return leaf_levels_; }

        // Returns the root of the page table. This is usually what ends up in
        // the Page Directory Base Register (PDBR / CR3).
        phys_t root() const { return page_alloc_.pointer_to_phys(root_); }

        // Return the mapping at the given virtual address.
        //
        // In case, the given virtual address corresponds to no mapping in the
        // page table, return an empty mapping (zero attributes).
        WARN_UNUSED_RESULT Mapping lookup(virt_t vaddr)
        {
            assert (root_ != nullptr);

            Mapping const result {lookup (vaddr, root_, max_levels_ - 1)};

            // The end of the final translation will wrap to zero.
            assert (result.vaddr <= vaddr and
                    ((result.vaddr + result.size() == 0) or (result.vaddr + result.size()) > vaddr));
            return result;
        }

        // Walk down the page table for a given virtual address.
        //
        // Walk down the page table to the indicated level and return a pointer
        // to the beginning of the page table. Splits any superpages and creates
        // missing page table structures (if desired).
        pte_pointer_t walk_down_and_split(DEFERRED_CLEANUP &cleanup,  virt_t vaddr,
                                          level_t to_level, bool create = true)
        {
            assert (root_ != nullptr);
            return walk_down_and_split (cleanup, vaddr, to_level, root_, max_levels_ - 1, create);
        }

        // Creates mappings in the page table. Returns true, if a TLB shootdown
        // is necessary.
        void update(DEFERRED_CLEANUP &cleanup, Mapping const &map)
        {
            assert (root_ != nullptr);
            assert (map.order >= PAGE_BITS and map.order <= max_order());

            ENTRY const align_mask {(static_cast<ENTRY>(1) << map.order) - 1};
            assert ((map.vaddr & align_mask) == 0);
            assert ((map.paddr & align_mask) == 0);

            // We have to modify one or more entries in this level and below.
            level_t modified_level {(map.order - PAGE_BITS) / BITS_PER_LEVEL};
            assert (modified_level < max_levels_);

            // Walk down the page table to find the relevant page table to
            // modify. If we encounter superpages on the way, split
            // them. Missing structures are only created, if we actually have
            // something to map.
            bool const do_create {map.present()};
            pte_pointer_t const table {walk_down_and_split (cleanup, map.vaddr,
                                                            modified_level, do_create)};

            // We skip filling in new entries when walk_down_and_split has
            // already finished the job. This happens when we remove mappings
            // and the walk down step did not found page tables to recurse into.
            if (table != nullptr) {
                fill_entries (cleanup, table, modified_level, map);
            }
        }

        // Convenience version of the above method when batching of TLB
        // invalidations is not required.
        DEFERRED_CLEANUP update(Mapping const &map)
        {
            DEFERRED_CLEANUP cleanup;

            update(cleanup, map);

            return cleanup;
        }

        // Replace a single non-existing or read-only page at the lowest page
        // table level with a new mapping.
        //
        // This function is safe to be used without synchronization to replace
        // read-only pages with writable pages. It does nothing, if there is an
        // existing writable mapping.
        //
        // The function returns the physical address that backs vaddr. This
        // address may not be the same as paddr, if another invocation of
        // replace_readonly_page replaced the entry concurrently.
        WARN_UNUSED_RESULT phys_t replace_readonly_page(DEFERRED_CLEANUP &cleanup, virt_t vaddr,
                                                        phys_t paddr, pte_t attr)
        {
            assert((paddr & ATTR::mask) == 0);
            assert((attr & ~ATTR::mask) == 0 and (attr & ATTR::PTE_P));

            pte_pointer_t const table {walk_down_and_split (cleanup, vaddr, 0, true)};
            assert(table != nullptr);

            pte_pointer_t const pte_p {table + virt_to_index(0, vaddr)};
            pte_t const new_pte {paddr | attr};
        retry:
            pte_t old_pte {memory_.read(pte_p)};

            if (old_pte != new_pte and (old_pte & ATTR::PTE_W) == 0) {
                if (not memory_.compare_exchange(pte_p, old_pte, new_pte)) {
                    goto retry;
                }

                old_pte = new_pte;
            }

            return old_pte & ~ATTR::mask;
        }

        // Prevent copying, but allow moving the page tables around.
        this_t &operator=(this_t const &rhs) = delete;
        Generic_page_table(this_t const &rhs) = delete;

        Generic_page_table(this_t &&rhs)
            : memory_ {rhs.memory_}, page_alloc_ {rhs.page_alloc_}, max_levels_ {rhs.max_levels_},
              leaf_levels_ {rhs.leaf_levels_}, root_ {rhs.root_}
        {
            rhs.root_ = nullptr;
        }

        // Create a new page table with a pre-existing root page table pointer.
        Generic_page_table(level_t max_levels, level_t leaf_levels, pte_pointer_t root, MEMORY const &memory = {})
            : memory_ {memory}, max_levels_ {max_levels},
              leaf_levels_ {leaf_levels}, root_ {root}
        {
            assert (leaf_levels_ > 0 and leaf_levels_ <= max_levels_);
            assert (max_levels_ > 0 and sizeof (virt_t) * 8 >= static_cast<size_t>(max_order()));
        }

        // Create an empty page table and allocate the root page table entry.
        Generic_page_table(level_t max_levels, level_t leaf_levels)
            : Generic_page_table (max_levels, leaf_levels, {}, {})
        {
            root_ = page_alloc_.alloc_zeroed_page();
        }

        // The destructor assumes that the page table is not in use anymore and
        // can be freed eagerly.
        ~Generic_page_table()
        {
            if (root_ == nullptr) {
                return;
            }

            DEFERRED_CLEANUP cleanup_state;
            cleanup_table(cleanup_state, root_, max_levels_);

            cleanup_state.ignore_tlb_flush();
            cleanup_state.free_pages_now();
        }
};
