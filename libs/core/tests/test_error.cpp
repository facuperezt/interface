#include <catch2/catch_test_macros.hpp>
#include "interface/core/error.hpp"

using namespace interface;

TEST_CASE("c_error::make captures source location", "[core][error]") {
    auto err = c_error::make("test error", e_error_category::io);

    REQUIRE(err.message == "test error");
    REQUIRE(err.category == e_error_category::io);
    REQUIRE(err.line > 0);
    REQUIRE_FALSE(err.file.empty());
}

TEST_CASE("c_error::format produces readable string", "[core][error]") {
    auto err = c_error::make("something failed", e_error_category::parse);
    auto formatted = err.format();

    REQUIRE(formatted.contains("something failed"));
    REQUIRE(formatted.contains(":"));
}

TEST_CASE("make_error returns std::unexpected", "[core][error]") {
    auto unexpected = make_error("bad input", e_error_category::parse);
    result_t<int> result = unexpected;

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().message == "bad input");
    REQUIRE(result.error().category == e_error_category::parse);
}

TEST_CASE("result_t holds value on success", "[core][error]") {
    result_t<int> result{42};

    REQUIRE(result.has_value());
    REQUIRE(result.value() == 42);
}

TEST_CASE("void_result_t works for void operations", "[core][error]") {
    void_result_t success{};
    REQUIRE(success.has_value());

    void_result_t failure = make_error("oops");
    REQUIRE_FALSE(failure.has_value());
    REQUIRE(failure.error().message == "oops");
}
