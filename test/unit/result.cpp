/*
 * Result Tests
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

#include "result.hpp"

#include <catch2/catch.hpp>

namespace
{

enum class Error
{
    SomeError,
    SomeOtherError,
};

using int_result = Result<int, Error>;
} // namespace

TEST_CASE("Basic construction works", "[result]")
{
    auto const ok_result{int_result::ok(12)};
    auto const err_result{int_result::err(Error::SomeOtherError)};

    REQUIRE(ok_result.is_ok());
    REQUIRE(err_result.is_err());

    CHECK(ok_result.unwrap() == 12);
    CHECK(ok_result.expect("should not fail") == 12);
    CHECK(err_result.unwrap_err() == Error::SomeOtherError);
}

TEST_CASE("Can instantiate Result with identical types", "[result]")
{
    auto ok_result{Result<int, int>::ok(12)};
    auto err_result{Result<int, int>::err(13)};

    CHECK(ok_result.unwrap() == 12);
    CHECK(err_result.unwrap_err() == 13);
}

TEST_CASE("Result copy construction works", "[result]")
{
    auto const source(int_result::err(Error::SomeOtherError));
    auto target{source};

    REQUIRE(target.is_err());
    CHECK(target.unwrap_err() == Error::SomeOtherError);
}

TEST_CASE("Result move construction works", "[result]")
{
    auto source(int_result::ok(12));
    auto target{move(source)};

    REQUIRE(target.is_ok());
    CHECK(target.unwrap() == 12);
}

TEST_CASE("Result unwrap_or_else works", "[result]")
{
    auto const ok_result{int_result::ok(17)};
    auto const err_result{int_result::err(Error::SomeOtherError)};

    CHECK(ok_result.unwrap_or_else([]() { return 1; }) == 17);
    CHECK(err_result.unwrap_or_else([]() { return 1; }) == 1);
}

TEST_CASE("Result map works", "[result]")
{
    auto const ok_result{int_result::ok(12)};
    auto const err_result{int_result::err(Error::SomeOtherError)};

    static char const* const message = "foo";
    auto const ok_mapped(ok_result.map([]([[maybe_unused]] int i) -> char const* { return message; }));

    REQUIRE(ok_mapped.is_ok());
    CHECK(ok_mapped.unwrap() == message);

    auto const err_mapped(err_result.map([]([[maybe_unused]] int i) -> char const* { return message; }));

    REQUIRE(err_mapped.is_err());
}

TEST_CASE("Result map_err works", "[result]")
{
    auto const ok_result{int_result::ok(12)};
    auto const err_result{int_result::err(Error::SomeOtherError)};

    static char const* const message = "foo";
    auto const ok_mapped(ok_result.map_err([]([[maybe_unused]] Error e) -> char const* { return message; }));

    REQUIRE(ok_mapped.is_ok());
    CHECK(ok_mapped.unwrap() == 12);

    auto const err_mapped(
        err_result.map_err([]([[maybe_unused]] Error e) -> char const* { return message; }));

    REQUIRE(err_mapped.is_err());
    CHECK(err_mapped.unwrap_err() == message);
}

TEST_CASE("Result and_then works", "[result]")
{
    auto ok_result{int_result::ok(12)};
    auto then_result{ok_result.and_then([](int i) -> int_result { return int_result::ok(i + 1); })};

    REQUIRE(then_result.is_ok());
    CHECK(then_result.unwrap() == 13);
}

TEST_CASE("TRY_OR_RETURN returns OK value", "[result]")
{
    auto ok_fn{[]() -> int_result { return Ok(17); }};
    auto test_fn{[ok_fn]() -> int_result { return Ok(TRY_OR_RETURN(ok_fn())); }};
    auto test_result{test_fn()};

    REQUIRE(test_result.is_ok());
    CHECK(test_result.unwrap() == 17);
}

TEST_CASE("TRY_OR_RETURN passes on error value", "[result]")
{
    auto err_fn{[]() -> int_result { return Err(Error::SomeError); }};
    auto test_fn{[err_fn]() -> int_result { return Ok(TRY_OR_RETURN(err_fn())); }};
    auto test_result{test_fn()};

    REQUIRE(test_result.is_err());
    CHECK(test_result.unwrap_err() == Error::SomeError);
}
