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

class Space_mem : public Space
{
    public:
        Hpt hpt;

        Dpt_new dpt;

        Ept_new ept;
        Hpt npt;

        mword did;

        Cpuset cpus;
        Cpuset htlb;
        Cpuset gtlb;

        static unsigned did_ctr;

        inline Space_mem(mword *boot_pt) : hpt (boot_pt), did (Atomic::add (did_ctr, 1U)) {}
        inline Space_mem(Hpt &src) : hpt (src.deep_copy (LINK_ADDR, SPC_LOCAL)), did (Atomic::add (did_ctr, 1U)) {}

        inline size_t lookup (mword virt, Paddr &phys)
        {
            auto m {hpt.lookup (virt)};
            phys = m.paddr | (virt & PAGE_MASK);
            return m.present() ? m.size() : 0;
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

        bool insert_utcb (mword, mword = 0);

        bool remove_utcb (mword);

        Tlb_cleanup update (Mdb *, mword = 0);

        static void shootdown();

        void init (unsigned);
};
