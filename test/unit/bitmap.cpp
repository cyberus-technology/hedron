/*
 * Generic Bitmap tests
 *
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
 *
 * This file is part of the Hedron microhypervisor.
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

// Include the class under test first to detect any missing includes early
#include <bitmap.hpp>

#include <catch2/catch.hpp>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>

TEST_CASE("Bit_accessor sets correct bits", "[bitmap]")
{
    constexpr size_t NUMBER_OF_BITS{128};

    std::vector<unsigned> bitmap_storage;
    bitmap_storage.assign(NUMBER_OF_BITS / sizeof(decltype(bitmap_storage)::value_type) / 8, 0);

    std::vector<unsigned> bitmap_compare = bitmap_storage;

    SECTION("Default 0")
    {
        Bitmap<unsigned, NUMBER_OF_BITS> bitmap(false);

        SECTION("First bit")
        {
            bitmap[0] = true;
            bitmap_compare.at(0) = 1;
            memcpy(bitmap_storage.data(), &bitmap,
                   sizeof(decltype(bitmap_storage)::value_type) * bitmap_storage.size());
            CHECK(bitmap_storage == bitmap_compare);
        }

        SECTION("Last bit")
        {
            bitmap[NUMBER_OF_BITS - 1] = true;
            bitmap_compare.back() = 1u << (sizeof(decltype(bitmap_storage)::value_type) * 8 - 1);
            memcpy(bitmap_storage.data(), &bitmap,
                   sizeof(decltype(bitmap_storage)::value_type) * bitmap_storage.size());
            CHECK(bitmap_storage == bitmap_compare);
        }

        SECTION("Guaranteed middle element")
        {
            bitmap[64] = true;
            bitmap_compare.at(2) = 1;
            memcpy(bitmap_storage.data(), &bitmap,
                   sizeof(decltype(bitmap_storage)::value_type) * bitmap_storage.size());
            CHECK(bitmap_storage == bitmap_compare);
        }

        SECTION("Odd number of bits")
        {
            bitmap_storage.push_back(0);
            Bitmap<unsigned, NUMBER_OF_BITS + 1> bitmap_odd(false);
            bitmap_odd[NUMBER_OF_BITS] = true;
            bitmap_compare.push_back(1);
            memcpy(bitmap_storage.data(), &bitmap_odd,
                   sizeof(decltype(bitmap_storage)::value_type) * bitmap_storage.size());
            CHECK(bitmap_storage == bitmap_compare);
        }
    }

    SECTION("Default 1")
    {
        Bitmap<unsigned, NUMBER_OF_BITS> bitmap(true);
        bitmap_compare.assign(bitmap_compare.size(), ~0u);

        SECTION("Clearing bits works")
        {
            bitmap[0] = false;
            bitmap[31] = false;
            bitmap_compare.at(0) = 0x7FFFFFFE;
            memcpy(bitmap_storage.data(), &bitmap,
                   sizeof(decltype(bitmap_storage)::value_type) * bitmap_storage.size());
            CHECK(bitmap_storage == bitmap_compare);
        }
    }
}

TEST_CASE("Bitmap iterators work", "[bitmap]")
{
    constexpr size_t SIZE{8};
    Bitmap<mword, SIZE> bitmap{false};

    SECTION("Iterators have basic sanity")
    {
        CHECK(bitmap.begin() == bitmap.begin());
        CHECK(bitmap.end() == bitmap.end());

        CHECK(++bitmap.begin() != bitmap.begin());

        CHECK(bitmap.begin() != bitmap.end());
        CHECK(bitmap.size() == SIZE);
    }

    SECTION("Iterators can be advanced")
    {
        auto it = bitmap.begin();
        std::advance(it, SIZE);
        CHECK(it == bitmap.end());
    }

    SECTION("Iterators can be referenced")
    {
        bitmap.set(1, true);
        CHECK_FALSE(*bitmap.begin());
        CHECK(*++bitmap.begin());
    }
}

TEST_CASE("Bitmap can be used as simple array of bits", "[bitmap]")
{
    constexpr size_t SIZE{8};

    SECTION("Inializer works")
    {
        Bitmap<mword, SIZE> bitmap_false{false};
        std::all_of(bitmap_false.begin(), bitmap_false.end(), [](bool b) { return not b; });

        Bitmap<mword, SIZE> bitmap_true{true};
        std::all_of(bitmap_true.begin(), bitmap_true.end(), [](bool b) { return b; });
    }

    SECTION("Set bit works")
    {
        Bitmap<mword, SIZE> bitmap{false};

        CHECK(not bitmap[5]);

        bitmap[5] = true;
        CHECK(bitmap[5]);
    }
}

TEST_CASE("Bitmap atomic operations work", "[bitmap]")
{
    // We make the bitmap larger than a single mword.
    constexpr size_t SIZE{128};
    Bitmap<mword, SIZE> bitmap{false};

    SECTION("atomic_fetch works")
    {
        CHECK(bitmap.atomic_fetch(100) == bitmap[100]);
        bitmap[100] = true;
        CHECK(bitmap.atomic_fetch(100) == bitmap[100]);
    }

    SECTION("atomic_fetch_set works")
    {
        CHECK(bitmap.atomic_fetch_set(100) == false);
        CHECK(bitmap[100] == true);
        CHECK(bitmap.atomic_fetch_set(100) == true);
    }

    SECTION("atomic_clear works")
    {
        // Clearing a cleared bit.
        bitmap[100].atomic_clear();
        CHECK(bitmap[100] == false);

        // Clearing a set bit.
        bitmap[100] = true;
        bitmap[100].atomic_clear();
        CHECK(bitmap[100] == false);
    }

    SECTION("atomic_union works")
    {
        Bitmap<mword, SIZE> empty_bitmap{false};
        Bitmap<mword, SIZE> other_bitmap{false};
        other_bitmap[17] = true;
        other_bitmap[100] = true;

        // Merging into a bitmap with all false values.
        bitmap.atomic_union(other_bitmap);
        CHECK(std::equal(bitmap.begin(), bitmap.end(), other_bitmap.begin(), other_bitmap.end()));

        // Merging all false values results in no change.
        bitmap.atomic_union(other_bitmap);
        CHECK(std::equal(bitmap.begin(), bitmap.end(), other_bitmap.begin(), other_bitmap.end()));

        // Merging in a single bit only changes that bit.
        Bitmap<mword, SIZE> single_bit{false};
        single_bit[7] = true;

        Bitmap<mword, SIZE> reference{false};
        reference[7] = true;
        reference[17] = true;
        reference[100] = true;

        bitmap.atomic_union(single_bit);
        CHECK(std::equal(bitmap.begin(), bitmap.end(), reference.begin(), reference.end()));
    }
}
