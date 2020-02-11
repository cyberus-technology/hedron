/*
 * Memory Space
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "config.hpp"
#include "cpu.hpp"
#include "cpuset.hpp"
#include "hpt.hpp"
#include "dpt.hpp"
#include "ept.hpp"
#include "space.hpp"
#include "tlb_cleanup.hpp"

class Space_mem
{
    public:
        Hpt hpt;

        Dpt dpt;

        Ept ept;
        Hpt npt;

        mword did;

        Cpuset cpus;
        Cpuset htlb;
        Cpuset gtlb;

        static unsigned did_ctr;

        // Constructor for the initial kernel memory space. The HPT doubles as
        // database, which memory is safe to give to userspace.
        Space_mem() : hpt (Hpt::make_golden_hpt()), did (Atomic::add (did_ctr, 1U)) {}

        // Constructor for normal memory spaces. The hpt parameter is the source
        // page table to populate kernel mappings.
        explicit Space_mem(Hpt &src) : hpt (src.deep_copy (LINK_ADDR, SPC_LOCAL)), did (Atomic::add (did_ctr, 1U)) {}

        inline bool lookup (mword virt, Paddr *phys)
        {
            return hpt.lookup_phys (virt, phys);
        }

        inline void insert (mword virt, unsigned o, mword attr, Paddr phys)
        {
            hpt.update ({virt, phys, attr, static_cast<Hpt::ord_t>(o + PAGE_BITS)});
        }

        inline Paddr replace (mword v, Paddr p)
        {
            return hpt.replace (v, p);
        }

        INIT
        void insert_root (uint64, uint64, mword = 0x7);

        // Claim a page for kernel use.
        //
        // Create a mapping for a physical memory region in the kernel page
        // tables and, if exclusive is true, prevent userspace from mapping this
        // page. order is given as byte order.
        void claim (mword virt, unsigned o, mword attr, Paddr phys, bool exclusive);

        // Convenience wrapper around claim() for single MMIO pages.
        void claim_mmio_page (mword virt, Paddr phys, bool exclusive = true);

        // Delegate memory from one memory space to another.
        Tlb_cleanup delegate (Space_mem *snd, mword snd_base, mword rcv_base, mword ord, mword attr, mword sub);

        // Revoke specific rights from a region of memory.
        Tlb_cleanup revoke (mword vaddr, mword ord, mword attr);

        static void shootdown();

        void init (unsigned);
};
