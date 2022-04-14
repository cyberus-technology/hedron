/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "compiler.hpp"
#include "types.hpp"

class Acpi_gas;
class Acpi_table_facs;

class Acpi
{
    friend class Acpi_table_fadt;
    friend class Acpi_table_rsdt;
    friend class Acpi_rsdp;
    friend class Hip;

private:
    enum Register
    {
        PM1_STS,
        PM1_ENA,
        PM1_CNT,
        PM2_CNT,
        GPE0_STS,
        GPE0_ENA,
        GPE1_STS,
        GPE1_ENA,
        PM_TMR,
        RESET
    };

    enum PM1_Status
    {
        PM1_STS_TMR = 1U << 0,        // 0x1
        PM1_STS_BM = 1U << 4,         // 0x10
        PM1_STS_GBL = 1U << 5,        // 0x20
        PM1_STS_PWRBTN = 1U << 8,     // 0x100
        PM1_STS_SLPBTN = 1U << 9,     // 0x200
        PM1_STS_RTC = 1U << 10,       // 0x400
        PM1_STS_PCIE_WAKE = 1U << 14, // 0x4000
        PM1_STS_WAKE = 1U << 15       // 0x8000
    };

    enum PM1_Enable
    {
        PM1_ENA_TMR = 1U << 0,       // 0x1
        PM1_ENA_GBL = 1U << 5,       // 0x20
        PM1_ENA_PWRBTN = 1U << 8,    // 0x100
        PM1_ENA_SLPBTN = 1U << 9,    // 0x200
        PM1_ENA_RTC = 1U << 10,      // 0x400
        PM1_ENA_PCIE_WAKE = 1U << 14 // 0x4000
    };

    enum PM1_Control
    {
        PM1_CNT_SLP_TYP_SHIFT = 10,

        PM1_CNT_SCI_EN = 1U << 0,                      // 0x1
        PM1_CNT_BM_RLD = 1U << 1,                      // 0x2
        PM1_CNT_GBL_RLS = 1U << 2,                     // 0x4
        PM1_CNT_SLP_TYP = 7U << PM1_CNT_SLP_TYP_SHIFT, // 0x1c00
        PM1_CNT_SLP_EN = 1U << 13                      // 0x2000
    };

    static unsigned const timer_frequency = 3579545;

    static Paddr dmar, facs, fadt, hpet, madt, mcfg, rsdt, xsdt;

    static Acpi_gas pm1a_sts;
    static Acpi_gas pm1b_sts;
    static Acpi_gas pm1a_ena;
    static Acpi_gas pm1b_ena;
    static Acpi_gas pm1a_cnt;
    static Acpi_gas pm1b_cnt;
    static Acpi_gas pm2_cnt;
    static Acpi_gas pm_tmr;
    static Acpi_gas gpe0_sts;
    static Acpi_gas gpe1_sts;
    static Acpi_gas gpe0_ena;
    static Acpi_gas gpe1_ena;
    static Acpi_gas reset_reg;

    static uint32 feature;
    static uint8 reset_val;

    static unsigned hw_read(Acpi_gas*);
    static unsigned read(Register);

    static void hw_write(Acpi_gas*, unsigned, bool = false);
    static void write(Register, unsigned);
    static void clear(Register, unsigned);

    static inline mword tmr_msb() { return feature & 0x100 ? 31 : 23; }

public:
    static unsigned irq;
    static unsigned gsi;

    static void delay(unsigned);
    static void reset();

    static Acpi_table_facs get_facs();
    static void set_facs(Acpi_table_facs const& saved_facs);

    // Sets the kind of execution mode that we want to wake up with.
    //
    // See set_waking_vector.
    enum class Wake_mode
    {
        REAL_MODE,
    };

    // Set the location of code that is executed when the system resumes
    // from a sleep state deeper than S1.
    static void set_waking_vector(Paddr vector, Wake_mode mode);

    // Return the value of the legacy wake vector.
    static Paddr get_waking_vector();

    // Check whether the SLP_TYP values look valid.
    static bool valid_sleep_type(uint8 slp_typa, uint8 slp_typb);

    // Enter an ACPI Sleep State.
    //
    // This function performs the final step of entering a sleep state by
    // writing the SLP_EN / SLP_TYPx bits as indicated by the respective
    // \_Sx system state DSDT method. See the ACPI specification, chapter
    // "System \_Sx States".
    //
    // The caller must ensure that the system is ready to enter the sleep
    // state as described in the ACPI specification, chapter "Waking and
    // Sleeping".
    //
    // Depending on the sleep state entered, this function might return (for
    // S1) or execution contines at the waking vector (S2, S3).
    static void enter_sleep_state(uint8 slp_typa, uint8 slp_typb);

    static void setup();

    // Initialize ACPI after all tables have been parsed.
    static void init();
};
