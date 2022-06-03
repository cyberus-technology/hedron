/*
 * Spinlock Tests
 *
 * Copyright (C) 2022 Julian Stecklina, Cyberus Technology GmbH.
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

#include "spinlock.hpp"

#include <algorithm>
#include <atomic>
#include <catch2/catch.hpp>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

TEST_CASE("Simple spinlock functionality", "[spinlock]")
{
    Spinlock l;

    CHECK(not l.is_locked());

    l.lock();
    CHECK(l.is_locked());
    l.unlock();

    CHECK(not l.is_locked());
}

TEST_CASE("Spinlock smoke test", "[spinlock]")
{
    static unsigned const thread_count{std::thread::hardware_concurrency()};

    Spinlock l;

    std::atomic<bool> should_exit{false};
    std::vector<std::thread> threads;

    std::atomic<uint64_t> unsafe_counter{0};
    std::atomic<uint64_t> safe_counter{0};

    for (size_t i = 0; i < thread_count; i++) {
        threads.emplace_back([&l, &should_exit, &unsafe_counter, &safe_counter]() {
            while (not should_exit.load(std::memory_order_relaxed)) {
                l.lock();

                // Increment unsafe_counter in an obviously concurrency unsafe way. This is fine as long as
                // the spinlock works. If not, we see it in the assertion below.
                unsafe_counter.store(unsafe_counter.load(std::memory_order_relaxed) + 1,
                                     std::memory_order_relaxed);

                // To see whether the above counter lost some updates, keep another counter around that we
                // increment the correct way.
                safe_counter++;

                l.unlock();
            }
        });
    }

    // We don't want to make the unit test run for long. In manual tests 100ms was more than enough to smoke
    // out an deliberately broken spinlock. This may not find subtle issues, though.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    should_exit = true;
    for (auto& t : threads) {
        t.join();
    }

    CHECK(unsafe_counter.load() == safe_counter.load());
}
