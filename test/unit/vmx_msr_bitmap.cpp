/*
 * VMX MSR bitmap tests
 *
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
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
#include <vmx_msr_bitmap.hpp>

#include <msr.hpp>

#include <catch2/catch.hpp>

#include <algorithm>
#include <cstring>

namespace {

alignas(0x1000) std::array<unsigned char, 0x1000> fake_bitmap_memory;

class Fake_page_alloc
{
    public:
        static void *alloc_zeroed_page()
        {
            memset(fake_bitmap_memory.data(), 0, fake_bitmap_memory.size());
            return fake_bitmap_memory.data();
        }

        static void free_page(void*) {}
};

auto all_ones = [](unsigned char byte) { return byte == 0xFF; };

} // anonymous namespace

TEST_CASE("MSR bitmap is all ones at init", "[vmx_msr_bitmap]")
{
    auto bmp {new Vmx_msr_bitmap<Fake_page_alloc>};
    CHECK(std::all_of(fake_bitmap_memory.begin(), fake_bitmap_memory.end(), all_ones));
}

TEST_CASE("MSR bitmap sets correct bits for low MSRs", "[vmx_msr_bitmap]")
{
    auto bmp {new Vmx_msr_bitmap<Fake_page_alloc>};

    const auto tsc_msr {Msr::Register::IA32_TSC};
    bmp->set_passthrough(tsc_msr);

    // We expect the 16th bit to be zero in the low read and low write ranges
    // That equals to
    //   bit 0 in the third byte (read)
    //   bit 0 in the 2051st byte (write)
    CHECK(std::all_of(fake_bitmap_memory.begin() +    0, fake_bitmap_memory.begin() + 2,    all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() +    3, fake_bitmap_memory.begin() + 2050, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 2051, fake_bitmap_memory.end(),          all_ones));
    CHECK(fake_bitmap_memory[2]    == 0xFE);
    CHECK(fake_bitmap_memory[2050] == 0xFE);

    // Now we set it to exit only on reads, but leave write passthrough
    bmp->set_read_exit(tsc_msr);

    CHECK(std::all_of(fake_bitmap_memory.begin() +    0, fake_bitmap_memory.begin() + 2050, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 2051, fake_bitmap_memory.end(),          all_ones));
    CHECK(fake_bitmap_memory[2050] == 0xFE);

    // Now we flip those two around
    bmp->set_read_passthrough(tsc_msr);
    bmp->set_write_exit(tsc_msr);

    CHECK(std::all_of(fake_bitmap_memory.begin() + 0, fake_bitmap_memory.begin() + 2, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 3, fake_bitmap_memory.end(),       all_ones));
    CHECK(fake_bitmap_memory[2] == 0xFE);
}

TEST_CASE("MSR bitmap sets correct bits for high MSRs", "[vmx_msr_bitmap]")
{
    auto bmp {new Vmx_msr_bitmap<Fake_page_alloc>};

    const auto star_msr {Msr::Register::IA32_STAR};
    bmp->set_passthrough(star_msr);

    // We expect the 129th bit to be zero in the high read and low write ranges
    // That equals to
    //   bit 1 in the 1041st byte (read)
    //   bit 1 in the 3089th byte (write)
    CHECK(std::all_of(fake_bitmap_memory.begin() +    0, fake_bitmap_memory.begin() + 1040, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 1041, fake_bitmap_memory.begin() + 3088, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 3089, fake_bitmap_memory.end(),          all_ones));
    CHECK(fake_bitmap_memory[1040] == 0xFD);
    CHECK(fake_bitmap_memory[3088] == 0xFD);

    // Now we set it to exit only on reads, but leave write passthrough
    bmp->set_read_exit(star_msr);

    CHECK(std::all_of(fake_bitmap_memory.begin() +    0, fake_bitmap_memory.begin() + 3088, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 3089, fake_bitmap_memory.end(),          all_ones));
    CHECK(fake_bitmap_memory[3088] == 0xFD);

    // Now we flip those two around
    bmp->set_read_passthrough(star_msr);
    bmp->set_write_exit(star_msr);

    CHECK(std::all_of(fake_bitmap_memory.begin() + 0, fake_bitmap_memory.begin() + 1040, all_ones));
    CHECK(std::all_of(fake_bitmap_memory.begin() + 1041, fake_bitmap_memory.end(),       all_ones));
    CHECK(fake_bitmap_memory[1040] == 0xFD);
}

TEST_CASE("MSR bitmap works at extremes", "[vmx_msr_bitmap]")
{
    auto bmp {new Vmx_msr_bitmap<Fake_page_alloc>};

    SECTION("MSR idx 0 read -> Bit 0") {
        bmp->set_read_passthrough(Msr::Register(0));
        CHECK(fake_bitmap_memory.front() == 0xFE);
        CHECK(std::all_of(fake_bitmap_memory.begin() + 1, fake_bitmap_memory.end(), all_ones));
    }

    SECTION("MSR idx 0xC0001FFF write -> Bit 32767") {
        bmp->set_write_passthrough(Msr::Register(0xC0001FFF));
        CHECK(fake_bitmap_memory.back() == 0x7F);
        CHECK(std::all_of(fake_bitmap_memory.rbegin() + 1, fake_bitmap_memory.rend(), all_ones));
    }

    SECTION("MSR idx 0xC0000000 -> first high MSR") {
        bmp->set_read_passthrough(Msr::Register(0xC0000000));
        bmp->set_write_passthrough(Msr::Register(0xC0000000));
        CHECK(std::all_of(fake_bitmap_memory.begin() +    0, fake_bitmap_memory.begin() + 1024, all_ones));
        CHECK(std::all_of(fake_bitmap_memory.begin() + 1025, fake_bitmap_memory.begin() + 3072, all_ones));
        CHECK(std::all_of(fake_bitmap_memory.begin() + 3073, fake_bitmap_memory.end(),          all_ones));
        CHECK(fake_bitmap_memory[1024] == 0xFE);
        CHECK(fake_bitmap_memory[3072] == 0xFE);
    }

    SECTION("MSR idx 0x1FFF -> last low MSR") {
        bmp->set_read_passthrough(Msr::Register(0x1FFF));
        bmp->set_write_passthrough(Msr::Register(0x1FFF));
        CHECK(std::all_of(fake_bitmap_memory.begin() +    0, fake_bitmap_memory.begin() + 1023, all_ones));
        CHECK(std::all_of(fake_bitmap_memory.begin() + 1024, fake_bitmap_memory.begin() + 3071, all_ones));
        CHECK(std::all_of(fake_bitmap_memory.begin() + 3072, fake_bitmap_memory.end(),          all_ones));
        CHECK(fake_bitmap_memory[1023] == 0x7F);
        CHECK(fake_bitmap_memory[3071] == 0x7F);
    }
}
