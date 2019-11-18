/*
 * Host page table modification
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

#include "hpt_new.hpp"
#include "nodestruct.hpp"
#include "mdb.hpp"
#include "pd.hpp"

Hpt_new::level_t Hpt_new::supported_leaf_levels {2};

Hpt_new Hpt_new::deep_copy(mword vaddr_start, mword vaddr_end)
{
    Hpt_new::Mapping map;
    Hpt_new dst;
    Tlb_cleanup cleanup;

    for (mword vaddr {vaddr_start}; vaddr < vaddr_end; vaddr += map.size()) {
        map = lookup (vaddr);

        if (not (map.present())) {
            continue;
        }

        // We don't handle the case where vaddr_start and vaddr_end point into
        // the middle mappings, but this case should also never happen.
        assert (map.vaddr >= vaddr_start and map.vaddr + map.size() <= vaddr_end);
        dst.update (cleanup, map);
    }

    // We populate an empty page table that is also not yet used anywhere.
    assert (not cleanup.need_tlb_flush());

    return dst;
}

Hpt_new &Hpt_new::boot_hpt()
{
    static No_destruct<Hpt_new> boot_hpt {reinterpret_cast<mword *>(&PDBRV)};

    return *&boot_hpt;
}

void *Hpt_new::remap (Paddr phys, bool use_boot_hpt)
{
    Tlb_cleanup cleanup;

    // We map 4MB in total: First the 2MB page where phys lands in and the next
    // one. This means the user of this function can safely access memory up to
    // 2MB.
    ord_t  const order {21};
    size_t const size {1UL << order};
    size_t const page_mask {size - 1};
    mword  const offset = phys & page_mask;

    phys &= ~page_mask;

    mword attr {Hpt_new::PTE_W | Hpt_new::PTE_P | Hpt_new::PTE_NX};

    // This manual distinction is unfortunate, but when creating the roottask
    // the current PD is not the boot page table anymore.
    Hpt_new &hpt {use_boot_hpt ? boot_hpt() : Pd::current()->hpt};

    hpt.update(cleanup, {SPC_LOCAL_REMAP,        phys,        attr, order});
    hpt.update(cleanup, {SPC_LOCAL_REMAP + size, phys + size, attr, order});

    // We always need to flush the TLB.
    hpt.flush();

    return reinterpret_cast<void *>(SPC_LOCAL_REMAP + offset);
}

Paddr Hpt_new::replace (mword vaddr, mword paddr)
{
    Tlb_cleanup cleanup;
    return replace_readonly_page (cleanup, vaddr, paddr & ~Hpt_new::mask, paddr & Hpt_new::mask);
}

Hpt_new::pte_t Hpt_new::hw_attr(mword a)
{
    if (a) {
        return Hpt_new::PTE_P | Hpt_new::PTE_U | Hpt_new::PTE_A | Hpt_new::PTE_D
            | (a & Mdb::MEM_W ? static_cast<mword>(Hpt_new::PTE_W) : 0)
            | (a & Mdb::MEM_X ? 0 : static_cast<mword>(Hpt_new::PTE_NX));
    }

    return 0;
}

void Hpt_new::set_supported_leaf_levels(Hpt_new::level_t level)
{
    assert (level > 0);
    supported_leaf_levels = level;
}
