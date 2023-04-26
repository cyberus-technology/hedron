/*
 * Generic Page Table Tests
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

// Include the class under test first to detect any missing includes early.
#include <generic_page_table.hpp>

#include <alloc_result.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <forward_list>
#include <initializer_list>

#include <catch2/catch.hpp>

namespace
{

// Non-autoconverting fake pointer for unit testing purposes. This is used
// together with Fake_memory and Fake_page_alloc.
template <typename ENTRY> class Generic_pointer
{
    using this_t = Generic_pointer<ENTRY>;

public:
    uint64_t addr{0};

    bool operator==(this_t const& rhs) const { return addr == rhs.addr; }

    bool operator==(std::nullptr_t) const { return addr == 0; }

    template <typename T> bool operator!=(T rhs) const { return not(*this == rhs); }

    this_t operator+(size_t offset) const { return {addr + offset * sizeof(ENTRY)}; }

    Generic_pointer(uint64_t addr_ = {}) : addr(addr_) {}
    Generic_pointer(std::nullptr_t) : Generic_pointer() {}
};

const unsigned BITS_PER_LEVEL_64BIT{9};
using entry = uint64_t;

// Our test makes no destinction between virtual and physical addresses. Doing
// so would complicate the test design, because the Fake_memory class would need
// to know the virtual-to-physical translation to record all memory accesses.
using pointer = Generic_pointer<entry>;

// Unit testing backend for the generic page table code's memory backend.
//
// Memory accesses made to this class are made to an abstract memory that can be
// inspected at any point in history. This is intended to be able to check
// certain invariants while memory is modified.
//
// In our specific case, we will use this to show that page table updates are
// indeed atomic in the sense that at any point during the modification the page
// table either contains the new or old translations and no broken intermediate
// state.
class Fake_memory
{
public:
    using entry = ::entry;
    using pointer = ::pointer;

private:
    using location = std::pair<pointer, entry>;

    using memory_list = std::forward_list<location>;
    using memory_it = memory_list::const_iterator;

    // Keep a list of address/value pairs that can be prepended with new
    // content.
    memory_list memory_;

public:
    // An iterator that iterates through the history of a memory object.
    //
    // This is somewhat slow as we need to copy the memory content for
    // each dereference.
    class iterator
    {
    private:
        Fake_memory const* memory_;
        memory_it it_;

    public:
        iterator& operator++()
        {
            ++it_;
            return *this;
        }

        Fake_memory operator*() const { return Fake_memory{{it_, memory_->memory_.cend()}}; }

        bool operator==(iterator const& rhs) const { return memory_ == rhs.memory_ and it_ == rhs.it_; }

        bool operator!=(iterator const& rhs) const { return not(*this == rhs); }

        iterator(Fake_memory const* memory) : memory_(memory), it_(memory->memory_.cbegin()) {}
    };

    // Returns an iterator to the current state of the memory. Can be
    // used to step through the past: increment is moving into the past.
    iterator now() const { return {this}; }

    Fake_memory(memory_list const& memory = {}) : memory_{memory} {}

    // The interface below is expected by Generic_page_table.

    entry read(pointer ptr) const
    {
        assert((ptr.addr & (sizeof(uint64_t) - 1)) == 0);

        auto it{std::find_if(memory_.cbegin(), memory_.cend(),
                             [ptr](auto const& pair) { return pair.first == ptr; })};

        if (it == memory_.cend()) {
            // Reading unwritten memory. Fake_page_alloc relies on this being
            // zero.
            return 0;
        }

        return it->second;
    }

    void write(pointer ptr, entry e) { memory_.emplace_front(ptr, e); }

    bool cmp_swap(pointer ptr, entry old, entry desired)
    {
        if (read(ptr) != old) {
            return false;
        }

        write(ptr, desired);
        return true;
    }

    entry exchange(pointer ptr, entry desired)
    {
        entry const old{read(ptr)};

        write(ptr, desired);

        return old;
    }
};

// A page allocator.
//
// The template parameter ALLOC_PAGE_COUNT can be used to artifically limit the number of pages that are
// handed out. It defaults to "infinity".
template <size_t ALLOC_PAGE_COUNT = ~0UL> class Fake_page_alloc
{
    uint64_t cur_alloc_{0x10000000};

    size_t allocated_pages_{0};

    using freed_memory_list = std::vector<uint64_t>;

    // All pages that were freed immediately.
    freed_memory_list freed_;

public:
    // The interface below is used by the unit test.

    size_t allocated_pages() const { return allocated_pages_; }

    freed_memory_list const& get_freed_pages() const { return freed_; }

    // The interface below is used by Generic_page_table.

    Alloc_result<pointer> alloc_zeroed_page()
    {
        assert(allocated_pages_ <= ALLOC_PAGE_COUNT);

        if (allocated_pages_ == ALLOC_PAGE_COUNT) {
            return Err(Out_of_memory_error{});
        }

        pointer cur{cur_alloc_};

        cur_alloc_ += PAGE_SIZE;
        allocated_pages_++;

        // We never give out memory twice, so there is no need to zero
        // it here. This only works because Fake_memory defaults to zero
        // for unused memory.

        return Ok(cur);
    }

    static pointer phys_to_pointer(entry e) { return {e}; }
    static entry pointer_to_phys(pointer p) { return p.addr; }

    void free_page(pointer ptr)
    {
        assert((pointer_to_phys(ptr) & PAGE_MASK) == 0);
        freed_.emplace_back(pointer_to_phys(ptr));
    }
};

class Fake_deferred_cleanup
{
    // Do we need to flush the TLB before freeing anything?
    bool tlb_flush_{false};

    using pointer_vector = std::vector<pointer>;
    pointer_vector lazy_free_pages_;

    Fake_deferred_cleanup(bool tlb_flush, pointer_vector const& lazy_free)
        : tlb_flush_{tlb_flush}, lazy_free_pages_{lazy_free}
    {
    }

public:
    // The testing interface

    pointer_vector get_freed_pages() const { return lazy_free_pages_; }

    // The interface expected by Generic_page_table

    Fake_deferred_cleanup() = default;

    WARN_UNUSED_RESULT bool need_tlb_flush() const { return tlb_flush_; }

    void ignore_tlb_flush() { tlb_flush_ = false; }
    void flush_tlb_later() { tlb_flush_ = true; }

    void merge(Fake_deferred_cleanup& other)
    {
        tlb_flush_ = tlb_flush_ or other.tlb_flush_;
        lazy_free_pages_.insert(lazy_free_pages_.end(), other.lazy_free_pages_.cbegin(),
                                other.lazy_free_pages_.cend());
    }

    void free_pages_now() { lazy_free_pages_ = {}; }

    static Fake_deferred_cleanup tlb_flush(bool tlb_flush) { return {tlb_flush, {}}; }

    void free_later(pointer page)
    {
        tlb_flush_ = true;
        lazy_free_pages_.emplace_back(page);
    }
};

class Fake_attr
{
public:
    enum : uint64_t
    {
        PTE_P = 1ULL << 0,
        PTE_W = 1ULL << 1,
        PTE_U = 1ULL << 2,
        PTE_S = 1ULL << 7,

        PTE_NX = 1ULL << 63,
    };

    static constexpr uint64_t mask{PTE_NX | PTE_P | PTE_W | PTE_U};
    static constexpr uint64_t all_rights{PTE_P | PTE_W | PTE_U};
};

using Fake_hpt = Generic_page_table<BITS_PER_LEVEL_64BIT, uint64_t, Fake_memory, Fake_page_alloc<>,
                                    Fake_deferred_cleanup, Fake_attr>;

Fake_hpt::ord_t const twomb_order{PAGE_BITS + BITS_PER_LEVEL_64BIT};
Fake_hpt::ord_t const onegb_order{PAGE_BITS + 2 * BITS_PER_LEVEL_64BIT};

template <size_t ALLOC_PAGE_COUNT>
using Memory_constrained_hpt =
    Generic_page_table<BITS_PER_LEVEL_64BIT, uint64_t, Fake_memory, Fake_page_alloc<ALLOC_PAGE_COUNT>,
                       Fake_deferred_cleanup, Fake_attr>;

// Given an iterator into the past of a page table, rewinds this
// page table to that time in the past.
Fake_hpt rewind(Fake_memory::iterator const& it, Fake_hpt const& source_hpt)
{
    return {source_hpt.max_levels(), source_hpt.leaf_levels(), source_hpt.root(), (*it)};
}

} // anonymous namespace

TEST_CASE("Empty page table lookup", "[page_table]")
{
    Fake_hpt hpt{4, 2};

    // An empty page table has an invalid mapping across one entry of the
    // top-most page table level.
    auto const empty_mapping{hpt.lookup(0)};
    CHECK(empty_mapping.attr == 0);
    CHECK(empty_mapping.order == (PAGE_BITS + 3 * BITS_PER_LEVEL_64BIT));
}

TEST_CASE("Basic page table walk works", "[page_table]")
{
    uint64_t const smallpage_vaddr{0x1000};
    uint64_t const smallpage_paddr{0xCAFE0000};

    uint64_t const superpage_vaddr{1U << (PAGE_BITS + BITS_PER_LEVEL_64BIT)};
    uint64_t const superpage_paddr{0x10000000};

    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::PTE_P},
                           {0x2000, 0x00003000 | Fake_attr::PTE_P},
                           {0x3000, 0x00004000 | Fake_attr::PTE_P},
                           {0x3008, superpage_paddr | Fake_attr::PTE_P | Fake_attr::PTE_S},
                           {0x4008, smallpage_paddr | Fake_attr::PTE_P}}};

    Fake_hpt hpt{4, 2, 0x1000, mem};

    SECTION("4K page can be walked")
    {
        auto const mapping{hpt.lookup(smallpage_vaddr)};

        CHECK(mapping.attr == Fake_attr::PTE_P);
        CHECK(mapping.vaddr == smallpage_vaddr);
        CHECK(mapping.paddr == smallpage_paddr);
        CHECK(mapping.order == PAGE_BITS);
    }

    SECTION("2MB superpage can be walked")
    {
        auto const mapping0{hpt.lookup(superpage_vaddr)};
        auto const mapping1{hpt.lookup(superpage_vaddr + PAGE_SIZE)};

        CHECK(mapping0.attr == Fake_attr::PTE_P);
        CHECK(mapping0.vaddr == superpage_vaddr);
        CHECK(mapping0.paddr == superpage_paddr);
        CHECK(mapping0.order == PAGE_BITS + BITS_PER_LEVEL_64BIT);

        CHECK(mapping0 == mapping1);
    }

    SECTION("lookup_phys handles non-existent mappings")
    {
        Fake_hpt::phys_t out{0};

        REQUIRE(not hpt.lookup_phys(smallpage_vaddr + PAGE_SIZE, &out));
    }

    SECTION("lookup_phys correctly handles 4K pages")
    {
        Fake_hpt::phys_t out{0};
        auto const success{hpt.lookup_phys(smallpage_vaddr + 0x123, &out)};

        REQUIRE(success);
        CHECK(out == smallpage_paddr + 0x123);
    }

    SECTION("lookup_phys correctly handles 2M pages")
    {
        Fake_hpt::phys_t out{0};
        auto const success{hpt.lookup_phys(superpage_vaddr + 0x123456, &out)};

        REQUIRE(success);
        CHECK(out == superpage_paddr + 0x123456);
    }
}

TEST_CASE("walk_down_and_split creates page table structures", "[page_table]")
{
    Fake_hpt hpt{4, 2};

    // Create all page table levels for virtual address zero.
    Fake_deferred_cleanup cleanup;
    hpt.walk_down_and_split(cleanup, 0, 0).unwrap();

    // Only upgrading entries from non-present to present requires no shootdown.
    CHECK_FALSE(cleanup.need_tlb_flush());

    auto const mapping{hpt.lookup(0)};

    CHECK(mapping.attr == 0);
    CHECK(mapping.order == PAGE_BITS);
}

TEST_CASE("Can split 2MB superpages", "[page_table]")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::PTE_P},
                           {0x2000, 0x00003000 | Fake_attr::PTE_P},
                           {0x3000, 0x10000000 | Fake_attr::PTE_P | Fake_attr::PTE_S}}};

    Fake_hpt hpt{4, 2, 0x1000, mem};

    // Create all page table levels for virtual address zero, splitting the
    // superpage on the way.
    Fake_deferred_cleanup cleanup;
    auto const table{hpt.walk_down_and_split(cleanup, 0, 0).unwrap()};
    REQUIRE(table != nullptr);

    SECTION("Page table structure was created correctly")
    {
        // Splitting superpages needs a TLB shootdown.
        CHECK(cleanup.need_tlb_flush());

        auto const mapping{hpt.lookup(0)};

        CHECK(mapping.attr == Fake_attr::PTE_P);
        CHECK(mapping.order == PAGE_BITS);
    }

    SECTION("New page table entries create identical mapping")
    {
        for (size_t i{0}; i < static_cast<size_t>(1) << BITS_PER_LEVEL_64BIT; i++) {
            auto const mapping{hpt.lookup(i * PAGE_SIZE)};

            CHECK(mapping.vaddr == i * PAGE_SIZE);
            CHECK(mapping.paddr == 0x10000000 + i * PAGE_SIZE);
            CHECK(mapping.attr == Fake_attr::PTE_P);
            CHECK(mapping.order == PAGE_BITS);
        }
    }
}

TEST_CASE("Can split 1GB pages into 2MB pages", "[page_table]")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::PTE_P},
                           {0x2000, 0x80000000 | Fake_attr::PTE_P | Fake_attr::PTE_S}}};

    Fake_hpt hpt{4, 3, 0x1000, mem};

    REQUIRE(hpt.lookup(0).order == onegb_order);

    Fake_deferred_cleanup cleanup;
    hpt.walk_down_and_split(cleanup, 0, 1).unwrap();
    CHECK(cleanup.need_tlb_flush());

    auto const mapping0{hpt.lookup(0)};
    auto const mapping1{hpt.lookup(static_cast<uint64_t>(1) << (PAGE_BITS + BITS_PER_LEVEL_64BIT))};

    CHECK(mapping0.vaddr == 0);
    CHECK(mapping0.paddr == 0x80000000);
    CHECK(mapping0.attr == Fake_attr::PTE_P);
    CHECK(mapping0.order == twomb_order);

    CHECK(mapping1.vaddr == 1U << twomb_order);
    CHECK(mapping1.paddr == 0x80000000 + (1U << twomb_order));
    CHECK(mapping1.attr == Fake_attr::PTE_P);
    CHECK(mapping1.order == twomb_order);
}

TEST_CASE("Update creates new mappings in empty page table", "[page_table]")
{
    Fake_hpt hpt{4, 3};

    auto create_and_check = [](Fake_hpt& hpt, Fake_hpt::virt_t vaddr, Fake_hpt::phys_t paddr,
                               Fake_hpt::ord_t order) {
        auto const cleanup{hpt.update({vaddr, paddr, Fake_attr::PTE_P | Fake_attr::PTE_W, order})};
        CHECK_FALSE(cleanup.need_tlb_flush());

        auto const mapping{hpt.lookup(vaddr)};

        CHECK(mapping.vaddr == vaddr);
        CHECK(mapping.paddr == paddr);
        CHECK(mapping.attr == (Fake_attr::PTE_P | Fake_attr::PTE_W));
        CHECK(mapping.order == order);
    };
    SECTION("Can create new 4K mapping")
    {
        Fake_hpt::ord_t const order{PAGE_BITS};
        create_and_check(hpt, 1UL << order, 0xDEADB000, order);
    }

    SECTION("Can create new 2MB mapping")
    {
        Fake_hpt::ord_t const order{PAGE_BITS + BITS_PER_LEVEL_64BIT};
        create_and_check(hpt, 1UL << order, 0xDEA00000, order);
    }

    SECTION("Can create new 1GB mapping")
    {
        Fake_hpt::ord_t const order{PAGE_BITS + 2 * BITS_PER_LEVEL_64BIT};
        create_and_check(hpt, 1UL << order, 0x80000000U, order);
    }
}

TEST_CASE("Update deals with out-of-memory errors", "[page_table]")
{
    // A page table that will only be able to allocate this many pages as page table backing store.
    using Hpt = Memory_constrained_hpt<3>;

    Hpt hpt{4, 3};
    Fake_deferred_cleanup cleanup;

    Hpt::virt_t const vaddr{0x1000};

    // We will be able to partially construct the page table.
    auto result{hpt.update(cleanup, {vaddr, 0x1000, Fake_attr::PTE_P, PAGE_BITS})};

    REQUIRE(result.is_err());

    // No mapping must be created.
    auto const mapping(hpt.lookup(vaddr));
    CHECK(not mapping.present());
}

TEST_CASE("Update creates no new page tables for unmap attempts", "[page_table]")
{
    SECTION("For an empty page table")
    {
        Fake_hpt hpt{4, 3};
        auto const cleanup{hpt.update({0, 0, 0, PAGE_BITS})};

        CHECK_FALSE(cleanup.need_tlb_flush());
        CHECK(hpt.lookup(0).order == PAGE_BITS + 3 * BITS_PER_LEVEL_64BIT);
        CHECK(hpt.page_alloc().allocated_pages() == 1);
    }

    SECTION("If unmapping a whole superpage")
    {
        Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::PTE_P},
                               {0x2000, 0x80000000 | Fake_attr::PTE_P | Fake_attr::PTE_S}}};

        Fake_hpt hpt{4, 3, 0x1000, mem};
        auto const cleanup{hpt.update({0, 0, 0, PAGE_BITS + 2 * BITS_PER_LEVEL_64BIT})};

        CHECK(cleanup.need_tlb_flush());
        CHECK(hpt.lookup(0).order == PAGE_BITS + 2 * BITS_PER_LEVEL_64BIT);
        CHECK(hpt.page_alloc().allocated_pages() == 0);
    }
}

TEST_CASE("Update splits superpages when unmapping", "[page_table]")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::PTE_P},
                           {0x2000, 0x80000000 | Fake_attr::PTE_P | Fake_attr::PTE_S}}};

    // Unmap 4K in a gigabyte page.
    Fake_hpt hpt{4, 3, 0x1000, mem};
    auto const cleanup{hpt.update({0, 0, 0, PAGE_BITS})};

    CHECK(cleanup.need_tlb_flush());

    // The region we actually unmapped.
    auto const mapping0{hpt.lookup(0)};
    CHECK(mapping0.order == PAGE_BITS);
    CHECK(mapping0.attr == 0);
    CHECK(mapping0.vaddr == 0);

    // The adjacent 4K region.
    auto const mapping1{hpt.lookup(PAGE_SIZE)};
    CHECK(mapping1.order == PAGE_BITS);
    CHECK(mapping1.attr == Fake_attr::PTE_P);
    CHECK(mapping1.vaddr == PAGE_SIZE);
    CHECK(mapping1.paddr == (0x80000000 + PAGE_SIZE));

    // The adjacent 2M region.
    auto const mapping2{hpt.lookup(1UL << twomb_order)};
    CHECK(mapping2.order == twomb_order);
    CHECK(mapping2.attr == (Fake_attr::PTE_P));
    CHECK(mapping2.vaddr == 1UL << twomb_order);
    CHECK(mapping2.paddr == (0x80000000 + (1UL << twomb_order)));
}

TEST_CASE("Updates are atomic", "[page_table]")
{
    SECTION("Normal updates are atomic")
    {
        Fake_hpt hpt{4, 3};

        Fake_hpt::Mapping const old_page_mapping{0, 0x80000000, Fake_attr::PTE_P, PAGE_BITS};
        Fake_hpt::Mapping const page_mapping{0, 0xC0000000, Fake_attr::PTE_P | Fake_attr::PTE_W, PAGE_BITS};

        CHECK_FALSE(hpt.update(old_page_mapping).need_tlb_flush());
        auto before_mapping{hpt.memory().now()};

        CHECK(hpt.update(page_mapping).need_tlb_flush());

        // We've mapped a single 4K page at virtual address 0. We
        // should see either the new or one old mapping at this position.
        for (auto it{hpt.memory().now()}; it != before_mapping; ++it) {
            auto past_hpt{rewind(it, hpt)};
            auto const cur_map{past_hpt.lookup(0)};

            REQUIRE((cur_map == old_page_mapping || cur_map == page_mapping));
        }
    }

    SECTION("Superpage splitting is atomic")
    {
        Fake_hpt hpt{4, 3};

        Fake_hpt::Mapping const old_gigabyte_mapping{0, 0x80000000, Fake_attr::PTE_P, onegb_order};
        Fake_hpt::Mapping const old_twomb_mapping{0, 0x80000000, Fake_attr::PTE_P, twomb_order};
        Fake_hpt::Mapping const old_page_mapping{0, 0x80000000, Fake_attr::PTE_P, PAGE_BITS};

        Fake_hpt::Mapping const page_mapping{0, 0xC0000000, Fake_attr::PTE_P | Fake_attr::PTE_W, PAGE_BITS};

        CHECK_FALSE(hpt.update(old_gigabyte_mapping).need_tlb_flush());
        auto before_mapping{hpt.memory().now()};

        CHECK(hpt.update(page_mapping).need_tlb_flush());

        // We've mapped a single 4K page at virtual address 0. We
        // should see either the new or one of the splitted versions
        // of the old mapping.
        for (auto it{hpt.memory().now()}; it != before_mapping; ++it) {
            auto past_hpt{rewind(it, hpt)};
            auto const cur_map{past_hpt.lookup(0)};

            REQUIRE((cur_map == old_gigabyte_mapping || cur_map == old_twomb_mapping ||
                     cur_map == old_page_mapping || cur_map == page_mapping));
        }
    }
}

TEST_CASE("Updates that changes mappings require TLB shootdowns", "[page_table]")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::all_rights},
                           {0x2000, 0x00003000 | Fake_attr::all_rights},
                           {0x3000, 0x00004000 | Fake_attr::all_rights},
                           {0x4000, 0x00000000 | Fake_attr::PTE_P},
                           {0x4008, 0x00000000 | Fake_attr::PTE_P | Fake_attr::PTE_W}}};

    Fake_hpt hpt{4, 3, 0x1000, mem};

    SECTION("Changing physical addresses requires a shootdown")
    {
        // Same rights, different physical address.
        CHECK(hpt.update({0, PAGE_SIZE, Fake_attr::PTE_P, PAGE_BITS}).need_tlb_flush());
    }

    SECTION("Downgrading rights requires a shootdown")
    {
        // Downgrade from PW -> P.
        CHECK(hpt.update({PAGE_SIZE, 0, Fake_attr::PTE_P, PAGE_BITS}).need_tlb_flush());
    }

    SECTION("Unmapping requires a shootdown")
    {
        // Downgrade from P -> ø.
        CHECK(hpt.update({0, 0, 0, PAGE_BITS}).need_tlb_flush());
    }

    // The update code could detect this case and avoid triggering a
    // TLB shootdown for performance, but this is currently not done.
    SECTION("Upgrading rights currently causes a shootdown")
    {
        // Upgrade from P -> PW.
        CHECK(hpt.update({0, 0, Fake_attr::PTE_P | Fake_attr::PTE_W, PAGE_BITS}).need_tlb_flush());
    }
}

TEST_CASE("Update that creates superpages reclaims page table structures")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::all_rights},
                           {0x2000, 0x00003000 | Fake_attr::all_rights},
                           {0x3000, 0x00004000 | Fake_attr::all_rights},
                           {0x3008, 0x00005000 | Fake_attr::all_rights}}};

    Fake_hpt hpt{4, 3, 0x1000, mem};

    SECTION("Mapping 2MB page over 4K pages lazily clears old page table structures")
    {
        auto const cleanup{hpt.update({0, 0, Fake_attr::PTE_P | Fake_attr::PTE_W, twomb_order})};
        CHECK(cleanup.need_tlb_flush());

        auto lazily_freed{cleanup.get_freed_pages()};
        auto immediately_freed{hpt.page_alloc().get_freed_pages()};

        REQUIRE(lazily_freed.size() == 1);
        REQUIRE(immediately_freed.size() == 0);

        // The leaf page table needs to be lazily freed.
        CHECK(lazily_freed[0] == 0x4000);
    }

    SECTION("Mapping 1GB page over 4K pages lazily clears old page table structures")
    {
        auto cleanup{hpt.update({0, 0, Fake_attr::PTE_P | Fake_attr::PTE_W, onegb_order})};
        CHECK(cleanup.need_tlb_flush());

        auto lazily_freed{cleanup.get_freed_pages()};
        auto immediately_freed{hpt.page_alloc().get_freed_pages()};

        REQUIRE(lazily_freed.size() == 3);
        REQUIRE(immediately_freed.size() == 0);

        // The two lowest levels of the page table need to be freed. Also the
        // leaf page should be freed first (this is not technically necessary
        // though).
        CHECK(lazily_freed[0] == 0x4000);
        CHECK(lazily_freed[1] == 0x5000);
        CHECK(lazily_freed[2] == 0x3000);
    }
}

TEST_CASE("Mapping memory works if it has to create multiple new page tables")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::all_rights},
                           {0x2008, 0x00003000 | Fake_attr::all_rights},
                           {0x3000, 0x00000000}}};

    // No superpage support
    Fake_hpt hpt{4, 1, 0x1000, mem};

    // This 4MB mapping at one gigabyte needs to create two new page table pages
    // at the lowest level.
    auto fourmb_order{twomb_order + 1};
    uint64_t virt{1 << onegb_order};
    auto cleanup{hpt.update({virt, 0, Fake_attr::PTE_P, fourmb_order})};

    CHECK_FALSE(cleanup.need_tlb_flush());

    for (size_t offset{0}; offset < 1U << fourmb_order; offset += PAGE_SIZE) {
        auto m{hpt.lookup(virt + offset)};

        REQUIRE(!!(m.attr & Fake_attr::PTE_P));
        REQUIRE(m.size() == PAGE_SIZE);
        REQUIRE(m.vaddr == virt + offset);
        REQUIRE(m.paddr == offset);
    }
}

TEST_CASE("Replacing read-only pages works", "[page_table]")
{
    Fake_memory const mem{{{0x1000, 0x00002000 | Fake_attr::all_rights},
                           {0x2000, 0x00003000 | Fake_attr::all_rights},
                           {0x3000, 0x00004000 | Fake_attr::all_rights},
                           {0x4000, 0x00001000 | Fake_attr::PTE_P},
                           {0x4008, 0x00002000 | Fake_attr::PTE_P | Fake_attr::PTE_W}}};

    Fake_hpt hpt{4, 3};
    Fake_deferred_cleanup cleanup;

    auto verify_mapping = [&hpt](uint64_t new_phys, uint64_t virt, uint64_t expected_phys) {
        CHECK(new_phys == expected_phys);

        auto const mapping{hpt.lookup(virt)};

        CHECK(mapping.present());
        CHECK(mapping.paddr == new_phys);
    };

    SECTION("Non-existing mapping is created")
    {
        verify_mapping(
            hpt.replace_readonly_page(cleanup, 0x4000, 0x3000, Fake_attr::PTE_P | Fake_attr::PTE_W), 0x4000,
            0x3000);
    }

    SECTION("Existing read-only page is replaced")
    {
        verify_mapping(hpt.replace_readonly_page(cleanup, 0, 0x3000, Fake_attr::PTE_P | Fake_attr::PTE_W), 0,
                       0x3000);
    }

    SECTION("Existing writable mapping is left as-is")
    {
        verify_mapping(
            hpt.replace_readonly_page(cleanup, 0x2000, 0x3000, Fake_attr::PTE_P | Fake_attr::PTE_W), 0x2000,
            0x3000);
    }
}

TEST_CASE("Clamping mappings works", "[page_table]")
{
    using Mapping = Fake_hpt::Mapping;

    // A 4MB "page" at 20MB.
    auto const attr{Fake_attr::PTE_P | Fake_attr::PTE_W};
    Mapping const source{5 << 22, 0, attr, 22};

    SECTION("Clamp is idempotent") { CHECK(source.clamp(source.vaddr, source.order) == source); }

    SECTION("Clamp preserves attributes") { CHECK(source.clamp(source.vaddr, source.order).attr == attr); }

    SECTION("Clamp into larger region is no-op")
    {
        // Clamp into 16MB page at 16MB.
        auto const clamped{source.clamp(4 << 22, 24)};

        CHECK(clamped == source);
    }

    SECTION("Clamp into smaller region returns smaller region")
    {
        // Clamp into 2MB page at 22MB;
        auto const clamped{source.clamp(22 << 20, 21)};

        CHECK(clamped.vaddr == 22 << 20);
        CHECK(clamped.paddr == 2 << 20);
        CHECK(clamped.order == 21);
    }
}

TEST_CASE("Moving mappings works", "[page_table]")
{
    using Mapping = Fake_hpt::Mapping;

    // A 4MB "page" at 20MB.
    Mapping const source{5 << 22, 0, Fake_attr::PTE_P, 22};

    SECTION("Moving by zero is a no-op") { CHECK(source.move_by(0) == source); }

    SECTION("Moving doesn't change start or attributes")
    {
        auto const moved{source.move_by(PAGE_SIZE)};

        CHECK(moved.vaddr == source.vaddr + PAGE_SIZE);
        CHECK(moved.paddr == source.paddr);
        CHECK(moved.attr == source.attr);
    }

    SECTION("Moving by small amount decreases mapping size")
    {
        auto const moved{source.move_by(PAGE_SIZE)};

        CHECK(moved.vaddr == source.vaddr + PAGE_SIZE);
        CHECK(moved.paddr == source.paddr);
        CHECK(moved.order == PAGE_BITS);
    }

    SECTION("Moving by matching offset doesn't change mapping size")
    {
        auto const moved{source.move_by(1UL << source.order)};

        CHECK(moved.vaddr == source.vaddr + (1UL << source.order));
        CHECK(moved.paddr == source.paddr);
        CHECK(moved.order == source.order);
    }

    SECTION("Moving by large amount doesn't change mapping size")
    {
        auto const moved{source.move_by(1UL << 30)};

        CHECK(moved.vaddr == source.vaddr + (1UL << 30));
        CHECK(moved.paddr == source.paddr);
        CHECK(moved.order == source.order);
    }
}
