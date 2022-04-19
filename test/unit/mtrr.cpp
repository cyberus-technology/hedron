/*
 * Memory Type Range Register Tests
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "generic_mtrr.hpp"
#include "msr.hpp"

#include <catch2/catch.hpp>

namespace
{

struct Fake_msr_base : private Msr {
public:
    using Msr::IA32_MTRR_CAP;
    using Msr::IA32_MTRR_DEF_TYPE;
    using Msr::IA32_MTRR_FIX16K_BASE;
    using Msr::IA32_MTRR_FIX4K_BASE;
    using Msr::IA32_MTRR_FIX64K_BASE;
    using Msr::IA32_MTRR_PHYS_BASE;
    using Msr::IA32_MTRR_PHYS_MASK;
    using Msr::Register;
};

// Fake MTRRs with fixed-range MTRRs as they are on a Kaby Lake NUC and
// variable-range as they are in the Intel SDM MTRR example (see Example 11-2 in
// "Memory Cache Control" in Intel SDM Vol. 3). For convenience, here is the
// description of the example:
//
// - 96 MBytes of system memory is mapped as write-back memory (WB) for highest
//   system performance.
// - A custom 4-MByte I/O card is mapped to uncached memory (UC) at a base
//   address of 64 MBytes. This restriction forces the 96 MBytes of system
//   memory to be addressed from 0 to 64 MBytes and from 68 MBytes to 100
//   MBytes, leaving a 4-MByte hole for the I/O card.
// - An 8-MByte graphics card is mapped to write-combining memory (WC) beginning
//   at address A0000000H.
// - The BIOS area from 15 MBytes to 16 MBytes is mapped to UC memory.
struct Fake_sdm_msr : public Fake_msr_base {
public:
    static uint64 read(Register r)
    {
        uint64 res = 0;

        switch (static_cast<uint32>(r)) {
        case 0x0fe:
            res = 0x0000000000001d0aULL;
            break;
        case 0x2ff:
            res = 0x0000000000000c00ULL;
            break;
        case 0x250:
            res = 0x0606060606060606ULL;
            break;
        case 0x258:
            res = 0x0606060606060606ULL;
            break;
        case 0x259:
            res = 0x0000000000000000ULL;
            break;
        case 0x268:
            res = 0x0505050505050505ULL;
            break;
        case 0x269:
            res = 0x0505050505050505ULL;
            break;
        case 0x26a:
            res = 0x0505050505050505ULL;
            break;
        case 0x26b:
            res = 0x0505050505050505ULL;
            break;
        case 0x26c:
            res = 0x0505050505050505ULL;
            break;
        case 0x26d:
            res = 0x0505050505050505ULL;
            break;
        case 0x26e:
            res = 0x0505050505050505ULL;
            break;
        case 0x26f:
            res = 0x0505050505050505ULL;
            break;
        case 0x200:
            res = 0x0000000000000006ULL;
            break;
        case 0x201:
            res = 0x0000000FFC000800ULL;
            break;
        case 0x202:
            res = 0x0000000004000006ULL;
            break;
        case 0x203:
            res = 0x0000000FFE000800ULL;
            break;
        case 0x204:
            res = 0x0000000006000006ULL;
            break;
        case 0x205:
            res = 0x0000000FFFC00800ULL;
            break;
        case 0x206:
            res = 0x0000000004000000ULL;
            break;
        case 0x207:
            res = 0x0000000FFFC00800ULL;
            break;
        case 0x208:
            res = 0x0000000000F00000ULL;
            break;
        case 0x209:
            res = 0x0000000FFFF00800ULL;
            break;
        case 0x20A:
            res = 0x00000000A0000001ULL;
            break;
        case 0x20B:
            res = 0x0000000FFF800800ULL;
            break;
        }

        return res;
    }
};

uint64 megabyte(size_t n) { return n << 20; }

} // anonymous namespace

TEST_CASE("NUC Fixed-range MTRRs are parsed correctly", "[mtrr]")
{
    using Mtrr_state = Generic_mtrr_state<Fake_sdm_msr>;

    Mtrr_state state;
    state.init();

    unsigned type;
    uint64 next;

    SECTION("Address 0 is RAM")
    {
        type = state.memtype(0, next);

        CHECK(type == 0x06);
    }

    SECTION("Video ROM is write-protected")
    {
        type = state.memtype(0xC0000, next);

        CHECK(type == 0x05);
    }

    SECTION("Legacy video memory is uncacheable")
    {
        type = state.memtype(0xB8000, next);

        CHECK(type == 0x00);
    }
}

TEST_CASE("NUC Variable-range MTRRs are parsed correctly in the SDM example", "[mtrr]")
{
    using Mtrr_state = Generic_mtrr_state<Fake_sdm_msr>;

    Mtrr_state state;
    state.init();

    unsigned type;
    uint64 next;

    SECTION("1-15M is WB (RAM)")
    {
        type = state.memtype(megabyte(1), next);

        CHECK(type == 0x06);
        CHECK(next == (megabyte(15)));
    }

    SECTION("15-16M is UC (BIOS Area)")
    {
        type = state.memtype(megabyte(15), next);

        CHECK(type == 0x00);
        CHECK(next == (megabyte(16)));
    }

    SECTION("16-64 is WB (RAM)")
    {
        type = state.memtype(megabyte(16), next);

        CHECK(type == 0x06);
        CHECK(next == (megabyte(64)));
    }

    SECTION("64-68 is UC (IO Card)")
    {
        type = state.memtype(megabyte(64), next);

        CHECK(type == 0x00);
        CHECK(next == megabyte(68));
    }

    SECTION("68-96 is WB (RAM)")
    {
        type = state.memtype(megabyte(68), next);

        CHECK(type == 0x06);
        CHECK(next == megabyte(96));
    }

    SECTION("96-100 is WB (RAM)")
    {
        type = state.memtype(megabyte(96), next);

        CHECK(type == 0x06);
        CHECK(next == megabyte(100));
    }

    SECTION("Gap has default memory type (UC)")
    {
        type = state.memtype(megabyte(100), next);

        CHECK(type == 0x00);
        CHECK(next == 0xA0000000U);
    }

    SECTION("Video RAM is WC")
    {
        type = state.memtype(0xA0000000U, next);

        CHECK(type == 0x01);
        CHECK(next == 0xA0000000U + megabyte(8));
    }
}
