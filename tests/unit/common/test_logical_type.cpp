#include <catch2/catch_test_macros.hpp>
#include "common/logical_type.h"

using namespace seeddb;

// =============================================================================
// LogicalTypeId enum tests
// =============================================================================

TEST_CASE("LogicalTypeId enum has all required types with correct values", "[logical_type]") {
    REQUIRE(static_cast<int>(LogicalTypeId::SQL_NULL) == 0);
    REQUIRE(static_cast<int>(LogicalTypeId::INTEGER) == 1);
    REQUIRE(static_cast<int>(LogicalTypeId::BIGINT) == 2);
    REQUIRE(static_cast<int>(LogicalTypeId::FLOAT) == 3);
    REQUIRE(static_cast<int>(LogicalTypeId::DOUBLE) == 4);
    REQUIRE(static_cast<int>(LogicalTypeId::VARCHAR) == 5);
    REQUIRE(static_cast<int>(LogicalTypeId::BOOLEAN) == 6);
}

// =============================================================================
// LogicalType class tests
// =============================================================================

TEST_CASE("LogicalType default constructs to SQL_NULL", "[logical_type]") {
    LogicalType type;
    REQUIRE(type.id() == LogicalTypeId::SQL_NULL);
}

TEST_CASE("LogicalType constructs from id", "[logical_type]") {
    LogicalType int_type(LogicalTypeId::INTEGER);
    REQUIRE(int_type.id() == LogicalTypeId::INTEGER);

    LogicalType varchar_type(LogicalTypeId::VARCHAR);
    REQUIRE(varchar_type.id() == LogicalTypeId::VARCHAR);

    LogicalType bool_type(LogicalTypeId::BOOLEAN);
    REQUIRE(bool_type.id() == LogicalTypeId::BOOLEAN);
}

// =============================================================================
// isNumeric tests
// =============================================================================

TEST_CASE("isNumeric returns true for numeric types", "[logical_type]") {
    REQUIRE(LogicalType(LogicalTypeId::INTEGER).isNumeric());
    REQUIRE(LogicalType(LogicalTypeId::BIGINT).isNumeric());
    REQUIRE(LogicalType(LogicalTypeId::FLOAT).isNumeric());
    REQUIRE(LogicalType(LogicalTypeId::DOUBLE).isNumeric());
}

TEST_CASE("isNumeric returns false for non-numeric types", "[logical_type]") {
    REQUIRE_FALSE(LogicalType(LogicalTypeId::SQL_NULL).isNumeric());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::VARCHAR).isNumeric());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::BOOLEAN).isNumeric());
}

// =============================================================================
// isInteger tests
// =============================================================================

TEST_CASE("isInteger returns true for integer types", "[logical_type]") {
    REQUIRE(LogicalType(LogicalTypeId::INTEGER).isInteger());
    REQUIRE(LogicalType(LogicalTypeId::BIGINT).isInteger());
}

TEST_CASE("isInteger returns false for non-integer types", "[logical_type]") {
    REQUIRE_FALSE(LogicalType(LogicalTypeId::SQL_NULL).isInteger());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::FLOAT).isInteger());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::DOUBLE).isInteger());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::VARCHAR).isInteger());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::BOOLEAN).isInteger());
}

// =============================================================================
// isFloating tests
// =============================================================================

TEST_CASE("isFloating returns true for floating-point types", "[logical_type]") {
    REQUIRE(LogicalType(LogicalTypeId::FLOAT).isFloating());
    REQUIRE(LogicalType(LogicalTypeId::DOUBLE).isFloating());
}

TEST_CASE("isFloating returns false for non-floating types", "[logical_type]") {
    REQUIRE_FALSE(LogicalType(LogicalTypeId::SQL_NULL).isFloating());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::INTEGER).isFloating());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::BIGINT).isFloating());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::VARCHAR).isFloating());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::BOOLEAN).isFloating());
}

// =============================================================================
// isString tests
// =============================================================================

TEST_CASE("isString returns true for VARCHAR", "[logical_type]") {
    REQUIRE(LogicalType(LogicalTypeId::VARCHAR).isString());
}

TEST_CASE("isString returns false for non-string types", "[logical_type]") {
    REQUIRE_FALSE(LogicalType(LogicalTypeId::SQL_NULL).isString());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::INTEGER).isString());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::BIGINT).isString());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::FLOAT).isString());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::DOUBLE).isString());
    REQUIRE_FALSE(LogicalType(LogicalTypeId::BOOLEAN).isString());
}

// =============================================================================
// fixedSize tests
// =============================================================================

TEST_CASE("fixedSize returns correct byte sizes for fixed-size types", "[logical_type]") {
    // INTEGER: 4 bytes (int32_t)
    REQUIRE(LogicalType(LogicalTypeId::INTEGER).fixedSize() == 4);

    // BIGINT: 8 bytes (int64_t)
    REQUIRE(LogicalType(LogicalTypeId::BIGINT).fixedSize() == 8);

    // FLOAT: 4 bytes (float)
    REQUIRE(LogicalType(LogicalTypeId::FLOAT).fixedSize() == 4);

    // DOUBLE: 8 bytes (double)
    REQUIRE(LogicalType(LogicalTypeId::DOUBLE).fixedSize() == 8);

    // BOOLEAN: 1 byte
    REQUIRE(LogicalType(LogicalTypeId::BOOLEAN).fixedSize() == 1);
}

TEST_CASE("fixedSize returns 0 for variable-length types", "[logical_type]") {
    // SQL_NULL: variable (0 for storage purposes)
    REQUIRE(LogicalType(LogicalTypeId::SQL_NULL).fixedSize() == 0);

    // VARCHAR: variable-length
    REQUIRE(LogicalType(LogicalTypeId::VARCHAR).fixedSize() == 0);
}
