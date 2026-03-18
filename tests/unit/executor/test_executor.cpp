#include <catch2/catch_test_macros.hpp>

#include "executor/executor.h"
#include "common/error.h"
#include "common/value.h"
#include "storage/row.h"

using namespace seeddb;

// =============================================================================
// ExecutionResult Tests
// =============================================================================

TEST_CASE("ExecutionResult: default constructor creates empty result", "[executor][execution_result]") {
    ExecutionResult result;

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    REQUIRE_FALSE(result.hasRow());
}

TEST_CASE("ExecutionResult: ok factory creates success result with row", "[executor][execution_result]") {
    Row row(std::vector<Value>{Value::integer(42), Value::varchar("hello")});
    ExecutionResult result = ExecutionResult::ok(std::move(row));

    REQUIRE(result.status() == ExecutionResult::Status::OK);
    REQUIRE(result.hasRow());
    REQUIRE(result.row().size() == 2);
    REQUIRE(result.row().get(0).asInt32() == 42);
    REQUIRE(result.row().get(1).asString() == "hello");
}

TEST_CASE("ExecutionResult: error factory creates error result", "[executor][execution_result]") {
    ExecutionResult result = ExecutionResult::error(ErrorCode::TABLE_NOT_FOUND, "Table 'users' not found");

    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE_FALSE(result.hasRow());
    REQUIRE(result.errorCode() == ErrorCode::TABLE_NOT_FOUND);
    REQUIRE(result.errorMessage() == "Table 'users' not found");
}

TEST_CASE("ExecutionResult: empty factory creates empty result", "[executor][execution_result]") {
    ExecutionResult result = ExecutionResult::empty();

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    REQUIRE_FALSE(result.hasRow());
}

TEST_CASE("ExecutionResult: ok result with empty row", "[executor][execution_result]") {
    Row empty_row;
    ExecutionResult result = ExecutionResult::ok(std::move(empty_row));

    REQUIRE(result.status() == ExecutionResult::Status::OK);
    REQUIRE(result.hasRow());
    REQUIRE(result.row().empty());
}

TEST_CASE("ExecutionResult: multiple error codes", "[executor][execution_result]") {
    SECTION("syntax error") {
        ExecutionResult result = ExecutionResult::error(ErrorCode::SYNTAX_ERROR, "Invalid syntax");
        REQUIRE(result.status() == ExecutionResult::Status::ERROR);
        REQUIRE(result.errorCode() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("internal error") {
        ExecutionResult result = ExecutionResult::error(ErrorCode::INTERNAL_ERROR, "Something went wrong");
        REQUIRE(result.status() == ExecutionResult::Status::ERROR);
        REQUIRE(result.errorCode() == ErrorCode::INTERNAL_ERROR);
    }

    SECTION("constraint violation") {
        ExecutionResult result = ExecutionResult::error(ErrorCode::CONSTRAINT_VIOLATION, "Primary key violation");
        REQUIRE(result.status() == ExecutionResult::Status::ERROR);
        REQUIRE(result.errorCode() == ErrorCode::CONSTRAINT_VIOLATION);
    }
}
