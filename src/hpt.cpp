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

#include "hpt.hpp"
#include "mdb.hpp"
#include "nodestruct.hpp"
#include "pd.hpp"

Hpt::level_t Hpt::supported_leaf_levels{2};

Hpt Hpt::deep_copy(mword vaddr_start, mword vaddr_end)
{
    Hpt::Mapping map;
    Hpt dst;
    Tlb_cleanup cleanup;

    for (mword vaddr{vaddr_start}; vaddr < vaddr_end; vaddr += map.size()) {
        map = lookup(vaddr);

        if (not(map.present())) {
            continue;
        }

        // We don't handle the case where vaddr_start and vaddr_end point into
        // the middle mappings, but this case should also never happen.
        assert(map.vaddr >= vaddr_start and map.vaddr + map.size() <= vaddr_end);

        dst.update(cleanup, map).unwrap("Failed to allocate memory during deep copy");
    }

    // We populate an empty page table that is also not yet used anywhere.
    assert(not cleanup.need_tlb_flush());

    return dst;
}

Hpt& Hpt::boot_hpt()
{
    static No_destruct<Hpt> boot_hpt{reinterpret_cast<mword*>(&PDBRV)};

    return *&boot_hpt;
}

const size_t Hpt::remap_guaranteed_size{0x200000};

void* Hpt::remap(Paddr phys, bool use_boot_hpt)
{
    Tlb_cleanup cleanup;

    // We map 4MB in total: First the 2MB page where phys lands in and the next
    // one. This means the user of this function can safely access memory up to
    // 2MB.
    ord_t const order{21};
    size_t const size{1UL << order};
    size_t const page_mask{size - 1};
    mword const offset = phys & page_mask;

    phys &= ~page_mask;

    mword attr{Hpt::PTE_W | Hpt::PTE_P | Hpt::PTE_NX};

    // This manual distinction is unfortunate, but when creating the roottask
    // the current PD is not the boot page table anymore.
    Hpt& hpt{use_boot_hpt ? boot_hpt() : Pd::current()->hpt};
    assert_slow(hpt.is_active());

    hpt.update(cleanup, {SPC_LOCAL_REMAP, phys, attr, order})
        .unwrap("Failed to allocate memory when remapping");
    hpt.update(cleanup, {SPC_LOCAL_REMAP + size, phys + size, attr, order})
        .unwrap("Failed to allocate memory when remapping");

    // We always need to flush the TLB.
    hpt.flush();

    return reinterpret_cast<void*>(SPC_LOCAL_REMAP + offset);
}

void Hpt::unmap_kernel_page(void* kernel_page)
{
    // See the function description in the header for an explanation why we only allow this operation on the
    // boot_hpt.
    Hpt& hpt{boot_hpt()};
    assert_slow(hpt.is_active());

    hpt.update({reinterpret_cast<mword>(kernel_page), Buddy::ptr_to_phys(kernel_page), 0, PAGE_BITS});
    Hpt::flush_one_page(kernel_page);
}

Paddr Hpt::replace(mword vaddr, mword paddr)
{
    Tlb_cleanup cleanup;
    return replace_readonly_page(cleanup, vaddr, paddr & ~Hpt::mask, paddr & Hpt::mask);
}

Hpt::pte_t Hpt::hw_attr(mword a)
{
    if (a) {
        return Hpt::PTE_P | Hpt::PTE_U | Hpt::PTE_A | Hpt::PTE_D |
               (a & Mdb::MEM_W ? static_cast<mword>(Hpt::PTE_W) : 0) |
               (a & Mdb::MEM_X ? 0 : static_cast<mword>(Hpt::PTE_NX));
    }

    return 0;
}

NOINLINE Hpt::pte_t Hpt::merge_hw_attr(Hpt::pte_t source, Hpt::pte_t desired)
{
    if (not(desired & source & Hpt::PTE_P)) {
        return 0;
    }

    pte_t const changeable{PTE_P | PTE_W | PTE_NX};

    // Invert NX bit to a positive permission bit that can be combined via AND.
    source ^= PTE_NX;
    desired ^= PTE_NX;

    return PTE_NX ^ ((source & ~changeable) | (source & desired & changeable));
}

void Hpt::set_supported_leaf_levels(Hpt::level_t level)
{
    assert(level > 0);
    supported_leaf_levels = level;
}
