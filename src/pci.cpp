/*
 * PCI Configuration Space
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
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

#include "algorithm.hpp"
#include "pci.hpp"
#include "pd.hpp"
#include "stdio.hpp"

INIT_PRIORITY (PRIO_SLAB)
Slab_cache Pci::cache (sizeof (Pci), 8);

unsigned    Pci::bus_base;
Paddr       Pci::cfg_base;
size_t      Pci::cfg_size;
Pci *       Pci::list;

struct Pci::quirk_map Pci::map[] =
{
};

Pci *Pci::find_dev (unsigned long r)
{
    auto range = Forward_list_range (list);
    auto it = find_if (range, [r] (auto const &pci) { return pci.rid == r; });

    return it != range.end() ? &*it : nullptr;
}

Pci::Pci (unsigned r, unsigned l) : Forward_list<Pci> (list), reg_base (hwdev_addr -= PAGE_SIZE), rid (static_cast<uint16>(r)), lev (static_cast<uint16>(l))
{
    Pd::kern->claim_mmio_page (reg_base, cfg_base + (rid << PAGE_BITS), false);

    for (unsigned i = 0; i < sizeof map / sizeof *map; i++)
        if (read<uint16>(REG_VID) == map[i].vid && read<uint16>(REG_DID) == map[i].did)
            (this->*map[i].func)();
}

void Pci::claim_all (Dmar *d)
{
    for_each (Forward_list_range {list}, [d] (Pci &pci) { if (not pci.dmar) { pci.dmar = d; } });
}

bool Pci::claim_dev (Dmar *d, unsigned r)
{
    Pci *pci = find_dev (r);

    if (!pci)
        return false;

    unsigned l = pci->lev;
    Forward_list_range devices {pci};

    // Find all following devices with a deeper level in the PCI hierarchy than
    // the current device. Deeper means a numerically larger level.
    assert (devices.begin() != devices.end());
    auto end = find_if (++devices.begin(), devices.end(),
                        [l] (Pci const &dev) { return dev.lev <= l; });

    for_each (devices.begin(), end, [d] (Pci &dev) { dev.dmar = d; });

    return true;
}

void Pci::init ()
{
    static constexpr unsigned MAX_BUS { 0xff };

    unsigned bus = 0;
    while (bus <= MAX_BUS) {
        unsigned max = Pci::scan (bus);
        bus = max ? max + 1 : bus + 1;
    }
}

unsigned Pci::scan (unsigned b, unsigned l, unsigned max_bus)
{
    unsigned current_max = max_bus > b ? max_bus : b;
    for (unsigned r = b << 8; r < (b + 1) << 8; r++) {
        if ((r << PAGE_BITS) >= cfg_size)
            return current_max;

        if (*static_cast<uint32 *>(Hpt::remap (cfg_base + (r << PAGE_BITS))) == ~0U)
            continue;

        Pci *p = new Pci (r, l);

        unsigned h = p->read<uint8>(REG_HDR);

        if ((h & HDR_TYPE) == PCI_BRIDGE) {
            unsigned new_max = scan (p->read<uint8>(REG_SBUSN), l + 1, current_max);
            current_max = new_max > current_max ? new_max : current_max;
        }

        // If the device b:d:0 has no multiple functions, skip the
        // remaining function slots and move on to the next device.
        if (!(r & BDF_FUNC) && !(h & HDR_MF))
            r += 7;
    }
    return current_max;
}
