/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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

#include "acpi_fadt.hpp"
#include "acpi.hpp"
#include "io.hpp"
#include "x86.hpp"

void Acpi_table_fadt::init(const Acpi_table_fadt* fadt)
{
    Acpi::feature = fadt->flags;

    Acpi::pm1a_sts.init(fadt->pm1a_sts());
    Acpi::pm1a_ena.init(fadt->pm1a_ena());
    Acpi::pm1b_sts.init(fadt->pm1b_sts());
    Acpi::pm1b_ena.init(fadt->pm1b_ena());

    Acpi::pm1a_cnt.init(fadt->pm1a_cnt());
    Acpi::pm1b_cnt.init(fadt->pm1b_cnt());
    Acpi::pm2_cnt.init(fadt->pm2_cnt());

    Acpi::pm_tmr.init(fadt->pm_tmr());

    Acpi::gpe0_sts.init(fadt->gpe0_sts());
    Acpi::gpe0_ena.init(fadt->gpe0_ena());
    Acpi::gpe1_sts.init(fadt->gpe1_sts());
    Acpi::gpe1_ena.init(fadt->gpe1_ena());

    if (fadt->length >= 129) {
        Acpi::reset_reg = fadt->reset_reg;
        Acpi::reset_val = fadt->reset_value;
    }

    Acpi::facs = fadt->facs();

    if (fadt->smi_cmd && fadt->acpi_enable) {
        Io::out(fadt->smi_cmd, fadt->acpi_enable);
        while (!(Acpi::read(Acpi::PM1_CNT) & Acpi::PM1_CNT_SCI_EN))
            pause();
    }
}
