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

#include "memory.hpp"
#include "types.hpp"

class Pci
{
    friend class Acpi_table_mcfg;
    friend class Hip;

private:
    static inline unsigned bus_base;
    static inline Paddr cfg_base;
    static inline size_t cfg_size;

public:
    static inline unsigned phys_to_rid(Paddr p)
    {
        return p - cfg_base < cfg_size ? static_cast<unsigned>((bus_base << 8) + (p - cfg_base) / PAGE_SIZE)
                                       : ~0U;
    }
};
