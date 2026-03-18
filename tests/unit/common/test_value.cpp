#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "common/value.h"

using Catch::Approx;

using namespace seeddb;

// =============================================================================
// Default constructor tests
// =============================================================================

TEST_CASE("Value default constructs to NULL", "[value]") {
    Value v;
    REQUIRE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::SQL_NULL);
}

// =============================================================================
// null() factory tests
// =============================================================================

TEST_CASE("null() factory creates NULL value", "[value]") {
    Value v = Value::null();
    REQUIRE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::SQL_NULL);
}

// =============================================================================
// integer() factory tests
// =============================================================================

TEST_CASE("integer() factory creates INTEGER with correct value", "[value]") {
    Value v = Value::integer(42);
    REQUIRE_FALSE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::INTEGER);
    REQUIRE(v.asInt32() == 42);
}

TEST_CASE("integer() factory handles negative values", "[value]") {
    Value v = Value::integer(-100);
    REQUIRE(v.asInt32() == -100);
}

TEST_CASE("integer() factory handles boundary values", "[value]") {
    Value min_val = Value::integer(std::numeric_limits<int32_t>::min());
    Value max_val = Value::integer(std::numeric_limits<int32_t>::max());
    REQUIRE(min_val.asInt32() == std::numeric_limits<int32_t>::min());
    REQUIRE(max_val.asInt32() == std::numeric_limits<int32_t>::max());
}

// =============================================================================
// bigint() factory tests
// =============================================================================

TEST_CASE("bigint() factory creates BIGINT with correct value", "[value]") {
    Value v = Value::bigint(1234567890123LL);
    REQUIRE_FALSE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::BIGINT);
    REQUIRE(v.asInt64() == 1234567890123LL);
}

TEST_CASE("bigint() factory handles negative values", "[value]") {
    Value v = Value::bigint(-9876543210LL);
    REQUIRE(v.asInt64() == -9876543210LL);
}

// =============================================================================
// Float() factory tests
// =============================================================================

TEST_CASE("Float() factory creates FLOAT with correct value", "[value]") {
    Value v = Value::Float(3.14f);
    REQUIRE_FALSE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::FLOAT);
    REQUIRE(v.asFloat() == Approx(3.14f));
}

TEST_CASE("Float() factory handles negative values", "[value]") {
    Value v = Value::Float(-2.5f);
    REQUIRE(v.asFloat() == Approx(-2.5f));
}

// =============================================================================
// Double() factory tests
// =============================================================================

TEST_CASE("Double() factory creates DOUBLE with correct value", "[value]") {
    Value v = Value::Double(3.14159265358979);
    REQUIRE_FALSE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::DOUBLE);
    REQUIRE(v.asDouble() == Approx(3.14159265358979));
}

TEST_CASE("Double() factory handles negative values", "[value]") {
    Value v = Value::Double(-1.23456789);
    REQUIRE(v.asDouble() == Approx(-1.23456789));
}

// =============================================================================
// varchar() factory tests
// =============================================================================

TEST_CASE("varchar() factory creates VARCHAR with correct value", "[value]") {
    Value v = Value::varchar("hello world");
    REQUIRE_FALSE(v.isNull());
    REQUIRE(v.typeId() == LogicalTypeId::VARCHAR);
    REQUIRE(v.asString() == "hello world");
}

TEST_CASE("varchar() factory handles empty string", "[value]") {
    Value v = Value::varchar("");
    REQUIRE(v.typeId() == LogicalTypeId::VARCHAR);
    REQUIRE(v.asString() == "");
}

// =============================================================================
// boolean() factory tests
// =============================================================================

TEST_CASE("boolean() factory creates BOOLEAN with correct value", "[value]") {
    Value v_true = Value::boolean(true);
    Value v_false = Value::boolean(false);

    REQUIRE_FALSE(v_true.isNull());
    REQUIRE(v_true.typeId() == LogicalTypeId::BOOLEAN);
    REQUIRE(v_true.asBool() == true);

    REQUIRE(v_false.typeId() == LogicalTypeId::BOOLEAN);
    REQUIRE(v_false.asBool() == false);
}

// =============================================================================
// type() accessor tests
// =============================================================================

TEST_CASE("type() returns LogicalType reference", "[value]") {
    Value v = Value::integer(42);
    const LogicalType& type = v.type();
    REQUIRE(type.id() == LogicalTypeId::INTEGER);
    REQUIRE(type.isNumeric());
    REQUIRE(type.isInteger());
}

// =============================================================================
// equals() tests
// =============================================================================

TEST_CASE("equals() returns true for equal NULL values", "[value]") {
    Value v1 = Value::null();
    Value v2 = Value::null();
    REQUIRE(v1.equals(v2));
    REQUIRE(v2.equals(v1));
}

TEST_CASE("equals() returns false for different types", "[value]") {
    Value v_int = Value::integer(42);
    Value v_bigint = Value::bigint(42);
    Value v_float = Value::Float(42.0f);
    Value v_double = Value::Double(42.0);
    Value v_str = Value::varchar("42");

    REQUIRE_FALSE(v_int.equals(v_bigint));
    REQUIRE_FALSE(v_int.equals(v_float));
    REQUIRE_FALSE(v_int.equals(v_double));
    REQUIRE_FALSE(v_int.equals(v_str));
}

TEST_CASE("equals() compares INTEGER values correctly", "[value]") {
    Value v1 = Value::integer(42);
    Value v2 = Value::integer(42);
    Value v3 = Value::integer(100);

    REQUIRE(v1.equals(v2));
    REQUIRE_FALSE(v1.equals(v3));
}

TEST_CASE("equals() compares BIGINT values correctly", "[value]") {
    Value v1 = Value::bigint(1234567890123LL);
    Value v2 = Value::bigint(1234567890123LL);
    Value v3 = Value::bigint(9876543210LL);

    REQUIRE(v1.equals(v2));
    REQUIRE_FALSE(v1.equals(v3));
}

TEST_CASE("equals() compares FLOAT values correctly", "[value]") {
    Value v1 = Value::Float(3.14f);
    Value v2 = Value::Float(3.14f);
    Value v3 = Value::Float(2.71f);

    REQUIRE(v1.equals(v2));
    REQUIRE_FALSE(v1.equals(v3));
}

TEST_CASE("equals() compares DOUBLE values correctly", "[value]") {
    Value v1 = Value::Double(3.14159265358979);
    Value v2 = Value::Double(3.14159265358979);
    Value v3 = Value::Double(2.71828);

    REQUIRE(v1.equals(v2));
    REQUIRE_FALSE(v1.equals(v3));
}

TEST_CASE("equals() compares VARCHAR values correctly", "[value]") {
    Value v1 = Value::varchar("hello");
    Value v2 = Value::varchar("hello");
    Value v3 = Value::varchar("world");

    REQUIRE(v1.equals(v2));
    REQUIRE_FALSE(v1.equals(v3));
}

TEST_CASE("equals() compares BOOLEAN values correctly", "[value]") {
    Value t1 = Value::boolean(true);
    Value t2 = Value::boolean(true);
    Value f1 = Value::boolean(false);
    Value f2 = Value::boolean(false);

    REQUIRE(t1.equals(t2));
    REQUIRE(f1.equals(f2));
    REQUIRE_FALSE(t1.equals(f1));
}

TEST_CASE("equals() returns false when comparing NULL with non-NULL", "[value]") {
    Value null_val = Value::null();
    Value int_val = Value::integer(42);

    REQUIRE_FALSE(null_val.equals(int_val));
    REQUIRE_FALSE(int_val.equals(null_val));
}

// =============================================================================
// lessThan() tests
// =============================================================================

TEST_CASE("lessThan() returns true when NULL compared to non-NULL", "[value]") {
    Value null_val = Value::null();
    Value int_val = Value::integer(42);

    REQUIRE(null_val.lessThan(int_val));
    REQUIRE_FALSE(int_val.lessThan(null_val));
}

TEST_CASE("lessThan() returns false when comparing two NULLs", "[value]") {
    Value v1 = Value::null();
    Value v2 = Value::null();
    REQUIRE_FALSE(v1.lessThan(v2));
    REQUIRE_FALSE(v2.lessThan(v1));
}

TEST_CASE("lessThan() uses type ID ordering for different types", "[value]") {
    // Type IDs: SQL_NULL=0, INTEGER=1, BIGINT=2, FLOAT=3, DOUBLE=4, VARCHAR=5, BOOLEAN=6
    Value int_val = Value::integer(1000);
    Value bigint_val = Value::bigint(1);
    Value float_val = Value::Float(0.001f);
    Value double_val = Value::Double(0.001);
    Value varchar_val = Value::varchar("a");
    Value bool_val = Value::boolean(false);

    // INTEGER (1) < BIGINT (2)
    REQUIRE(int_val.lessThan(bigint_val));
    REQUIRE_FALSE(bigint_val.lessThan(int_val));

    // BIGINT (2) < FLOAT (3)
    REQUIRE(bigint_val.lessThan(float_val));
    REQUIRE_FALSE(float_val.lessThan(bigint_val));

    // FLOAT (3) < DOUBLE (4)
    REQUIRE(float_val.lessThan(double_val));
    REQUIRE_FALSE(double_val.lessThan(float_val));

    // DOUBLE (4) < VARCHAR (5)
    REQUIRE(double_val.lessThan(varchar_val));
    REQUIRE_FALSE(varchar_val.lessThan(double_val));

    // VARCHAR (5) < BOOLEAN (6)
    REQUIRE(varchar_val.lessThan(bool_val));
    REQUIRE_FALSE(bool_val.lessThan(varchar_val));
}

TEST_CASE("lessThan() compares INTEGER values correctly", "[value]") {
    Value v1 = Value::integer(10);
    Value v2 = Value::integer(20);
    Value v3 = Value::integer(10);

    REQUIRE(v1.lessThan(v2));
    REQUIRE_FALSE(v2.lessThan(v1));
    REQUIRE_FALSE(v1.lessThan(v3));
}

TEST_CASE("lessThan() compares BIGINT values correctly", "[value]") {
    Value v1 = Value::bigint(100);
    Value v2 = Value::bigint(200);

    REQUIRE(v1.lessThan(v2));
    REQUIRE_FALSE(v2.lessThan(v1));
}

TEST_CASE("lessThan() compares FLOAT values correctly", "[value]") {
    Value v1 = Value::Float(1.5f);
    Value v2 = Value::Float(2.5f);

    REQUIRE(v1.lessThan(v2));
    REQUIRE_FALSE(v2.lessThan(v1));
}

TEST_CASE("lessThan() compares DOUBLE values correctly", "[value]") {
    Value v1 = Value::Double(1.1);
    Value v2 = Value::Double(2.2);

    REQUIRE(v1.lessThan(v2));
    REQUIRE_FALSE(v2.lessThan(v1));
}

TEST_CASE("lessThan() compares VARCHAR values correctly", "[value]") {
    Value v1 = Value::varchar("apple");
    Value v2 = Value::varchar("banana");

    REQUIRE(v1.lessThan(v2));
    REQUIRE_FALSE(v2.lessThan(v1));
}

TEST_CASE("lessThan() compares BOOLEAN values correctly (false < true)", "[value]") {
    Value v_false = Value::boolean(false);
    Value v_true = Value::boolean(true);

    REQUIRE(v_false.lessThan(v_true));
    REQUIRE_FALSE(v_true.lessThan(v_false));
}

// =============================================================================
// toString() tests
// =============================================================================

TEST_CASE("toString() returns 'NULL' for null values", "[value]") {
    Value v = Value::null();
    REQUIRE(v.toString() == "NULL");
}

TEST_CASE("toString() returns string representation of INTEGER", "[value]") {
    REQUIRE(Value::integer(42).toString() == "42");
    REQUIRE(Value::integer(-100).toString() == "-100");
    REQUIRE(Value::integer(0).toString() == "0");
}

TEST_CASE("toString() returns string representation of BIGINT", "[value]") {
    REQUIRE(Value::bigint(1234567890123LL).toString() == "1234567890123");
    REQUIRE(Value::bigint(-9876543210LL).toString() == "-9876543210");
}

TEST_CASE("toString() returns string representation of FLOAT", "[value]") {
    // Note: exact string format may vary, just check it's not empty
    REQUIRE_FALSE(Value::Float(3.14f).toString().empty());
}

TEST_CASE("toString() returns string representation of DOUBLE", "[value]") {
    // Note: exact string format may vary, just check it's not empty
    REQUIRE_FALSE(Value::Double(3.14159).toString().empty());
}

TEST_CASE("toString() returns string representation of VARCHAR", "[value]") {
    REQUIRE(Value::varchar("hello").toString() == "hello");
    REQUIRE(Value::varchar("").toString() == "");
}

TEST_CASE("toString() returns 'true' or 'false' for BOOLEAN", "[value]") {
    REQUIRE(Value::boolean(true).toString() == "true");
    REQUIRE(Value::boolean(false).toString() == "false");
}
