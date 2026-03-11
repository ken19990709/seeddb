#include <catch2/catch_test_macros.hpp>
#include "common/error.h"

TEST_CASE("ErrorCode values are defined", "[error]") {
    REQUIRE(static_cast<int>(seeddb::ErrorCode::SUCCESS) == 0);
    REQUIRE(static_cast<int>(seeddb::ErrorCode::UNKNOWN_ERROR) != 0);
}

TEST_CASE("Error can be constructed with code and message", "[error]") {
    seeddb::Error err(seeddb::ErrorCode::SYNTAX_ERROR, "Invalid SQL syntax");
    REQUIRE(err.code() == seeddb::ErrorCode::SYNTAX_ERROR);
    REQUIRE(err.message() == "Invalid SQL syntax");
}

TEST_CASE("Error::what() returns formatted message", "[error]") {
    seeddb::Error err(seeddb::ErrorCode::TABLE_NOT_FOUND, "users");
    std::string what = err.what();
    REQUIRE(what.find("TABLE_NOT_FOUND") != std::string::npos);
    REQUIRE(what.find("users") != std::string::npos);
}

TEST_CASE("Error::ok() returns true for SUCCESS code", "[error]") {
    seeddb::Error ok_err(seeddb::ErrorCode::SUCCESS, "");
    seeddb::Error bad_err(seeddb::ErrorCode::UNKNOWN_ERROR, "oops");

    REQUIRE(ok_err.ok() == true);
    REQUIRE(bad_err.ok() == false);
}

TEST_CASE("Error can be thrown and caught", "[error]") {
    REQUIRE_THROWS_AS(
        throw seeddb::Error(seeddb::ErrorCode::INTERNAL_ERROR, "test"),
        seeddb::Error
    );
}

TEST_CASE("Result<T> holds value on success", "[error]") {
    seeddb::Result<int> result = seeddb::Result<int>::ok(42);
    REQUIRE(result.ok() == true);
    REQUIRE(result.value() == 42);
}

TEST_CASE("Result<T> holds error on failure", "[error]") {
    seeddb::Result<int> result = seeddb::Result<int>::err(
        seeddb::ErrorCode::TYPE_ERROR, "expected int"
    );
    REQUIRE(result.ok() == false);
    REQUIRE(result.error().code() == seeddb::ErrorCode::TYPE_ERROR);
}

TEST_CASE("Result<void> works for void returns", "[error]") {
    seeddb::Result<void> ok_result = seeddb::Result<void>::ok();
    seeddb::Result<void> err_result = seeddb::Result<void>::err(
        seeddb::ErrorCode::IO_ERROR, "disk full"
    );

    REQUIRE(ok_result.is_ok() == true);
    REQUIRE(err_result.is_ok() == false);
}

TEST_CASE("error_code_name returns correct names", "[error]") {
    REQUIRE(std::string(seeddb::error_code_name(seeddb::ErrorCode::SUCCESS)) == "SUCCESS");
    REQUIRE(std::string(seeddb::error_code_name(seeddb::ErrorCode::SYNTAX_ERROR)) == "SYNTAX_ERROR");
    REQUIRE(std::string(seeddb::error_code_name(seeddb::ErrorCode::TABLE_NOT_FOUND)) == "TABLE_NOT_FOUND");
}
