/*
 * ACPI FADT Parse Tests
 *
 * Copyright (C) 2022 Sebastian Eydam, Cyberus Technology GmbH.
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

#include "acpi_fadt.hpp"
#include "acpi_fadt_test_helpers.hpp"

#include <catch2/catch.hpp>

#include <functional>

// This unit test checks whether the ACPI registers, that are contained in the FADT, are parsed correctly.

namespace
{
// This function compares the parsed values of a given fadt with the desired values for that FADT.
void check_gas_fields(const Gas_values& desired_values, const void* test_fadt)
{
    const Acpi_table_fadt* fadt_table{static_cast<const Acpi_table_fadt*>(test_fadt)};

    CHECK(desired_values.pm1a_sts == fadt_table->pm1a_sts());
    CHECK(desired_values.pm1a_ena == fadt_table->pm1a_ena());
    CHECK(desired_values.pm1b_sts == fadt_table->pm1b_sts());
    CHECK(desired_values.pm1b_ena == fadt_table->pm1b_ena());
    CHECK(desired_values.pm1a_cnt == fadt_table->pm1a_cnt());
    CHECK(desired_values.pm1b_cnt == fadt_table->pm1b_cnt());

    CHECK(desired_values.pm2_cnt == fadt_table->pm2_cnt());
    CHECK(desired_values.pm_tmr == fadt_table->pm_tmr());

    CHECK(desired_values.gpe0_sts == fadt_table->gpe0_sts());
    CHECK(desired_values.gpe0_ena == fadt_table->gpe0_ena());
    CHECK(desired_values.gpe1_sts == fadt_table->gpe1_sts());
    CHECK(desired_values.gpe1_ena == fadt_table->gpe1_ena());
}
} // anonymous namespace

TEST_CASE("ACPI registers are parsed correctly")
{
    // Each test compares the parsing result of a given FADT with the desired result for the given FADT.

    SECTION("ACPI registers of the XPS 13 are parsed correctly")
    {
        check_gas_fields(xps_gas_values, fadt_xps.data());
    }

    SECTION("ACPI registers of the Tuxedo laptop are parsed correctly")
    {
        check_gas_fields(tuxedo_gas_values, fadt_tuxedo.data());
    }

    SECTION("ACPI registers of a Qemu virtual machine are parsed correctly")
    {
        check_gas_fields(qemu_gas_values, fadt_qemu.data());
    }
}
