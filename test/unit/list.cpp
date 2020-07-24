/*
 * List Tests
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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

#include "list.hpp"

#include <catch2/catch.hpp>

#include <algorithm>
#include <iostream>

namespace {

class Element : public Forward_list<Element>
{
    public:
        int value;

        Element (Element *&head, int value_)
            : Forward_list<Element> {head}, value {value_}
        {}
};

class Test_list
{
        // A vector that keeps track of Elements in order to clean them up.
        std::vector<std::unique_ptr<Element>> backing_store;

    public:
        Element *head = nullptr;

        Test_list (std::initializer_list<int> const &init)
        {
            for (int v : init) {
                backing_store.emplace_back (std::make_unique<Element>(head, v));
            }
        }
};

// This operator compares Elements to integers by their value.
bool operator==(Forward_list_range<Element> const &range, std::vector<int> const &vector)
{
    return std::equal (range.begin(), range.end(),
                       vector.begin(), vector.end(),
                       [] (Element const &el, int i) { return el.value == i; });
}

}

TEST_CASE ("Forward_list iterators works", "[list]")
{
    Test_list test_list {1, 2, 3};
    auto range {Forward_list_range (test_list.head)};

    auto cur {std::begin (range)};

    CHECK (cur->value == 1);
    CHECK ((++cur)->value == 2);
    CHECK ((++cur)->value == 3);

    CHECK ((++cur) == std::end (range));
}

TEST_CASE ("Empty lists work", "[list]")
{
    Element *el {nullptr};
    auto range {Forward_list_range (el)};

    CHECK (std::begin (range) == std::end (range));
}

TEST_CASE ("Algorithms work on lists", "[list]")
{
    Test_list test_list {1, 2, 3};

    CHECK (Forward_list_range (test_list.head) == std::vector<int> {1, 2, 3});
}

TEST_CASE("Range-based for works on lists", "[list]")
{
    Test_list test_list {1, 2, 3};

    CHECK (Forward_list_range (test_list.head) == std::vector<int> {1, 2, 3});
}
