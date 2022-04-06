/*
 * DMA Remapping Unit (DMAR)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "dmar.hpp"
#include "dpt.hpp"
#include "lapic.hpp"
#include "pd.hpp"
#include "stdio.hpp"
#include "vectors.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Dmar::cache(sizeof(Dmar), 8);

Dmar* Dmar::list;
Dmar_ctx* Dmar::ctx = new Dmar_ctx;
Dmar_irt* Dmar::irt = new Dmar_irt;
uint32 Dmar::gcmd = GCMD_TE;

Dmar::Dmar(Paddr p)
    : Forward_list<Dmar>(list), reg_base((hwdev_addr -= PAGE_SIZE) | (p & PAGE_MASK)),
      invq(static_cast<Dmar_qi*>(Buddy::allocator.alloc(ord, Buddy::FILL_0))), invq_idx(0)
{
    Pd::kern->claim_mmio_page(reg_base, p & ~PAGE_MASK);

    cap = read<uint64>(REG_CAP);
    ecap = read<uint64>(REG_ECAP);

    // Bit n in this mask means that n can be a leaf level.
    mword const leaf_bit_mask{1U /* 4K */ | ((static_cast<mword>(cap >> 34) & 0b11) << 1)};
    auto const leaf_levels{static_cast<Dpt::level_t>(bit_scan_reverse(leaf_bit_mask) + 1)};

    assert(page_table_levels() >= leaf_levels);
    Dpt::lower_supported_leaf_levels(leaf_levels);

    if (ir()) {
        gcmd |= GCMD_IRE;
    }

    if (qi()) {
        gcmd |= GCMD_QIE;
    }

    // FIXME: This is too early to know whether IR should be enabled. See #158.
    init();
}

void Dmar::init()
{
    write<uint32>(REG_FEADDR, 0xfee00000 | Cpu::apic_id[0] << 12);
    write<uint32>(REG_FEDATA, VEC_MSI_DMAR);
    write<uint32>(REG_FECTL, 0);

    write<uint64>(REG_RTADDR, Buddy::ptr_to_phys(ctx));
    command(GCMD_SRTP);

    if (ire()) {
        write<uint64>(REG_IRTA, Buddy::ptr_to_phys(irt) | 7);
        command(GCMD_SIRTP);
    }

    if (qie()) {
        invq_idx = 0;
        write<uint64>(REG_IQT, 0);
        write<uint64>(REG_IQA, Buddy::ptr_to_phys(invq));
        command(GCMD_QIE);
    }
}

int Dmar::page_table_levels() const
{
    long const lev{2 + bit_scan_reverse((cap >> 8) & 0b00110)};
    assert(lev >= 3);

    return static_cast<int>(lev);
}

void Dmar::assign(unsigned long rid, Pd* p)
{
    Dmar_ctx* r = ctx + (rid >> 8);
    int const lev{page_table_levels()};

    if (!r->present())
        r->set(0, Buddy::ptr_to_phys(new Dmar_ctx) | 1);

    Dmar_ctx* c = static_cast<Dmar_ctx*>(Buddy::phys_to_ptr(r->addr())) + (rid & 0xff);
    if (c->present())
        c->set(0, 0);

    flush_ctx();

    auto const root{p->dpt.root(lev)};
    auto const address_width{static_cast<mword>(lev) - 2};

    c->set(address_width | p->did << 8, root | 1);
}

void Dmar::fault_handler()
{
    for (uint32 fsts; fsts = read<uint32>(REG_FSTS), fsts & 0xff;) {

        if (fsts & 0x2) {
            uint64 hi, lo;
            for (unsigned frr = fsts >> 8 & 0xff; read(frr, hi, lo), hi & 1ull << 63; frr = (frr + 1) % nfr())
                trace(TRACE_IOMMU, "DMAR:%p FRR:%u FR:%#x BDF:%x:%x:%x FI:%#010llx", this, frr,
                      static_cast<uint32>(hi >> 32) & 0xff, static_cast<uint32>(hi >> 8) & 0xff,
                      static_cast<uint32>(hi >> 3) & 0x1f, static_cast<uint32>(hi) & 0x7, lo);
        }

        write<uint32>(REG_FSTS, 0x7d);
    }
}

void Dmar::vector(unsigned vector)
{
    unsigned msi = vector - VEC_MSI;

    if (EXPECT_TRUE(msi == 0)) {
        for_each(Forward_list_range{list}, mem_fn_closure(&Dmar::fault_handler)());
    }

    Lapic::eoi();
}
