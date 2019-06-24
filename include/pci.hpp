/*
 * PCI Configuration Space
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "io.hpp"
#include "list.hpp"
#include "slab.hpp"

class Dmar;

class Pci : public List<Pci>
{
    friend class Acpi_table_mcfg;
    friend class Hip;

    private:
        mword  const        reg_base;
        uint16 const        rid;
        uint16 const        lev;
        Dmar *              dmar;

        static unsigned     bus_base;
        static Paddr        cfg_base;
        static size_t       cfg_size;

        static Pci *        list;
        static Slab_cache   cache;

        static struct quirk_map
        {
            uint16 vid, did;
            void (Pci::*func)();
        } map[];

        enum Register
        {
            REG_VID         = 0x0,
            REG_DID         = 0x2,
            REG_HDR         = 0xe,
            REG_SBUSN       = 0x19,
            REG_MAX         = 0xfff,
        };

        enum Mask
        {
            BDF_FUNC = 0x7,
            HDR_TYPE = 0x7f,
            HDR_MF   = 0x80,
        };

        enum Type
        {
            GENERAL    = 0,
            PCI_BRIDGE = 1,
        };

        template <typename T>
        inline unsigned read (Register r) { return *reinterpret_cast<T volatile *>(reg_base + r); }

        template <typename T>
        inline void write (Register r, T v) { *reinterpret_cast<T volatile *>(reg_base + r) = v; }

        static inline Pci *find_dev (unsigned long r)
        {
            for (Pci *pci = list; pci; pci = pci->next)
                if (pci->rid == r)
                    return pci;

            return nullptr;
        }

    public:
        INIT
        Pci (unsigned, unsigned);

        static inline void *operator new (size_t) { return cache.alloc(); }

        static inline void claim_all (Dmar *d)
        {
            for (Pci *pci = list; pci; pci = pci->next)
                if (!pci->dmar)
                    pci->dmar = d;
        }

        static inline bool claim_dev (Dmar *d, unsigned r)
        {
            Pci *pci = find_dev (r);

            if (!pci)
                return false;

            unsigned l = pci->lev;
            do pci->dmar = d; while ((pci = pci->next) && pci->lev > l);

            return true;
        }

        INIT
        static void init ();

        INIT
        static unsigned scan (unsigned bus = 0, unsigned level = 0, unsigned max_bus = 0);

        static inline unsigned phys_to_rid (Paddr p)
        {
            return p - cfg_base < cfg_size ? static_cast<unsigned>((bus_base << 8) + (p - cfg_base) / PAGE_SIZE) : ~0U;
        }

        static inline Dmar *find_dmar (unsigned long r)
        {
            Pci *pci = find_dev (r);

            return pci ? pci->dmar : nullptr;
        }
};
