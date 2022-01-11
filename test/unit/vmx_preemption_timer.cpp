/*
 * VMX Preemption Timer tests
 *
 * Copyright (C) 2021 Stefan Hertrampf, Cyberus Technology GmbH.
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

// Include the class under test first to detect any missing includes early
#include <vmx_preemption_timer.hpp>

#include <catch2/catch.hpp>

TEST_CASE("tsc_to_timer_value_calculation_works", "[vmx_preemption_timer]")
{
    // Zero stays zero and is not rounded up
    CHECK(vmx_timer::calc_timeout(0, 0) == 0);
    CHECK(vmx_timer::calc_timeout(0, 5) == 0);

    // If shift is zero we do not lose precision
    CHECK(vmx_timer::calc_timeout(0xffff, 0) == 0xffff);
    CHECK(vmx_timer::calc_timeout(0xffffffff, 0) == 0xffffffff);

    // If shift is present but no information is lost due to shift we do not
    // round up
    for (int i{0}; i < 4; i++) {
        CHECK(vmx_timer::calc_timeout(0xf0, i) == (0xf0 >> i));
    }

    // We do not overflow the available 32bits
    CHECK(vmx_timer::calc_timeout(~0ull, 0) == 0xffffffff);
    CHECK(vmx_timer::calc_timeout(~0ull, 1) == 0xffffffff);
    CHECK(vmx_timer::calc_timeout(~0ull, 31) == 0xffffffff);

    // Rounding up works
    for (int i{0xf1}; i < 0x100; i++) {
        CHECK(vmx_timer::calc_timeout(i, 4) == 0x10);
    }
}
