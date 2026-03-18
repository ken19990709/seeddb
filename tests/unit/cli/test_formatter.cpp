#include <catch2/catch_test_macros.hpp>

#include "cli/formatter.h"
#include "common/logical_type.h"
#include "common/value.h"
#include "storage/row.h"
#include "storage/schema.h"

using namespace seeddb;
using namespace seeddb::cli;

// =============================================================================
// Helper Builders
// =============================================================================

static Schema makeSchema(std::vector<std::pair<std::string, LogicalTypeId>> cols) {
    std::vector<ColumnSchema> columns;
    for (auto& [name, type] : cols) {
        columns.emplace_back(name, LogicalType(type));
    }
    return Schema(std::move(columns));
}

static Row makeRow(std::vector<Value> values) {
    return Row(std::move(values));
}

// =============================================================================
// Empty / Zero-column Schema
// =============================================================================

TEST_CASE("TableFormatter: empty schema returns (0 rows)", "[formatter]") {
    Schema schema;
    std::vector<Row> rows;
    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result == "(0 rows)\n");
}

// =============================================================================
// Row Count Footer
// =============================================================================

TEST_CASE("TableFormatter: footer shows singular 'row' for 1 row", "[formatter]") {
    Schema schema = makeSchema({{"id", LogicalTypeId::INTEGER}});
    std::vector<Row> rows = {makeRow({Value::integer(1)})};

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("(1 row)") != std::string::npos);
    REQUIRE(result.find("(1 rows)") == std::string::npos);
}

TEST_CASE("TableFormatter: footer shows plural 'rows' for 0 rows", "[formatter]") {
    Schema schema = makeSchema({{"id", LogicalTypeId::INTEGER}});
    std::vector<Row> rows;

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("(0 rows)") != std::string::npos);
}

TEST_CASE("TableFormatter: footer shows plural 'rows' for multiple rows", "[formatter]") {
    Schema schema = makeSchema({{"id", LogicalTypeId::INTEGER}});
    std::vector<Row> rows = {
        makeRow({Value::integer(1)}),
        makeRow({Value::integer(2)}),
        makeRow({Value::integer(3)})
    };

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("(3 rows)") != std::string::npos);
}

// =============================================================================
// Border / Header Structure
// =============================================================================

TEST_CASE("TableFormatter: output contains border lines", "[formatter]") {
    Schema schema = makeSchema({{"name", LogicalTypeId::VARCHAR}});
    std::vector<Row> rows = {makeRow({Value::varchar("Alice")})};

    std::string result = TableFormatter::format(schema, rows);
    // Border line should start with '+' and end with '+'
    REQUIRE(result.find("+") != std::string::npos);
    REQUIRE(result.find("+-") != std::string::npos);
    REQUIRE(result.find("-+") != std::string::npos);
}

TEST_CASE("TableFormatter: output contains column header", "[formatter]") {
    Schema schema = makeSchema({{"username", LogicalTypeId::VARCHAR}});
    std::vector<Row> rows;

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("username") != std::string::npos);
    REQUIRE(result.find("| username |") != std::string::npos);
}

TEST_CASE("TableFormatter: output contains multiple column headers", "[formatter]") {
    Schema schema = makeSchema({
        {"id", LogicalTypeId::INTEGER},
        {"name", LogicalTypeId::VARCHAR},
        {"active", LogicalTypeId::BOOLEAN}
    });
    std::vector<Row> rows;

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("id") != std::string::npos);
    REQUIRE(result.find("name") != std::string::npos);
    REQUIRE(result.find("active") != std::string::npos);
}

// =============================================================================
// Column Width Calculation
// =============================================================================

TEST_CASE("TableFormatter: column width uses header length when header is wider", "[formatter]") {
    // Header "username" (8 chars) > data "Al" (2 chars)
    Schema schema = makeSchema({{"username", LogicalTypeId::VARCHAR}});
    std::vector<Row> rows = {makeRow({Value::varchar("Al")})};

    std::string result = TableFormatter::format(schema, rows);
    // Header cell should be padded to at least "username" width
    REQUIRE(result.find("| username |") != std::string::npos);
    // Data cell should also be padded to same width
    REQUIRE(result.find("| Al       |") != std::string::npos);
}

TEST_CASE("TableFormatter: column width uses data length when data is wider", "[formatter]") {
    // Header "id" (2 chars) < data "1000000" (7 chars)
    Schema schema = makeSchema({{"id", LogicalTypeId::INTEGER}});
    std::vector<Row> rows = {makeRow({Value::integer(1000000)})};

    std::string result = TableFormatter::format(schema, rows);
    // Header "id" should be padded to "1000000" width (7 chars)
    REQUIRE(result.find("| id      |") != std::string::npos);
    REQUIRE(result.find("| 1000000 |") != std::string::npos);
}

TEST_CASE("TableFormatter: column width uses max data length across all rows", "[formatter]") {
    // Header "v" (1 char), data: "1" (1), "100" (3), "10" (2) → max is 3
    Schema schema = makeSchema({{"v", LogicalTypeId::INTEGER}});
    std::vector<Row> rows = {
        makeRow({Value::integer(1)}),
        makeRow({Value::integer(100)}),
        makeRow({Value::integer(10)})
    };

    std::string result = TableFormatter::format(schema, rows);
    // All cells should align to width 3
    REQUIRE(result.find("| v   |") != std::string::npos);
    REQUIRE(result.find("| 1   |") != std::string::npos);
    REQUIRE(result.find("| 100 |") != std::string::npos);
    REQUIRE(result.find("| 10  |") != std::string::npos);
}

// =============================================================================
// NULL Value Display
// =============================================================================

TEST_CASE("TableFormatter: NULL values display as 'NULL'", "[formatter]") {
    Schema schema = makeSchema({{"name", LogicalTypeId::VARCHAR}});
    std::vector<Row> rows = {makeRow({Value::null()})};

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("NULL") != std::string::npos);
}

TEST_CASE("TableFormatter: NULL column width counts 'NULL' string length", "[formatter]") {
    // Header "n" (1 char) < "NULL" (4 chars)
    Schema schema = makeSchema({{"n", LogicalTypeId::VARCHAR}});
    std::vector<Row> rows = {makeRow({Value::null()})};

    std::string result = TableFormatter::format(schema, rows);
    // Column width should be 4 (length of "NULL")
    REQUIRE(result.find("| n    |") != std::string::npos);
    REQUIRE(result.find("| NULL |") != std::string::npos);
}

// =============================================================================
// Various Value Types
// =============================================================================

TEST_CASE("TableFormatter: formats INTEGER values", "[formatter]") {
    Schema schema = makeSchema({{"num", LogicalTypeId::INTEGER}});
    std::vector<Row> rows = {makeRow({Value::integer(42)})};

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("42") != std::string::npos);
}

TEST_CASE("TableFormatter: formats BIGINT values", "[formatter]") {
    Schema schema = makeSchema({{"big", LogicalTypeId::BIGINT}});
    std::vector<Row> rows = {makeRow({Value::bigint(9876543210LL)})};

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("9876543210") != std::string::npos);
}

TEST_CASE("TableFormatter: formats VARCHAR values", "[formatter]") {
    Schema schema = makeSchema({{"msg", LogicalTypeId::VARCHAR}});
    std::vector<Row> rows = {makeRow({Value::varchar("hello world")})};

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("hello world") != std::string::npos);
}

TEST_CASE("TableFormatter: formats BOOLEAN values as true/false", "[formatter]") {
    Schema schema = makeSchema({{"flag", LogicalTypeId::BOOLEAN}});
    std::vector<Row> rows = {
        makeRow({Value::boolean(true)}),
        makeRow({Value::boolean(false)})
    };

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("true") != std::string::npos);
    REQUIRE(result.find("false") != std::string::npos);
}

// =============================================================================
// Multi-column Table
// =============================================================================

TEST_CASE("TableFormatter: formats multi-column table correctly", "[formatter]") {
    Schema schema = makeSchema({
        {"id", LogicalTypeId::INTEGER},
        {"name", LogicalTypeId::VARCHAR}
    });
    std::vector<Row> rows = {
        makeRow({Value::integer(1), Value::varchar("Alice")}),
        makeRow({Value::integer(2), Value::varchar("Bob")})
    };

    std::string result = TableFormatter::format(schema, rows);
    REQUIRE(result.find("id") != std::string::npos);
    REQUIRE(result.find("name") != std::string::npos);
    REQUIRE(result.find("1") != std::string::npos);
    REQUIRE(result.find("Alice") != std::string::npos);
    REQUIRE(result.find("2") != std::string::npos);
    REQUIRE(result.find("Bob") != std::string::npos);
    REQUIRE(result.find("(2 rows)") != std::string::npos);
}

// =============================================================================
// Single Row Overload
// =============================================================================

TEST_CASE("TableFormatter: single row overload formats correctly", "[formatter]") {
    Schema schema = makeSchema({{"id", LogicalTypeId::INTEGER}});
    Row row = makeRow({Value::integer(99)});

    std::string result = TableFormatter::format(schema, row);
    REQUIRE(result.find("99") != std::string::npos);
    REQUIRE(result.find("(1 row)") != std::string::npos);
}

TEST_CASE("TableFormatter: single row overload with empty row shows 0 rows", "[formatter]") {
    Schema schema = makeSchema({{"id", LogicalTypeId::INTEGER}});
    Row empty_row;

    std::string result = TableFormatter::format(schema, empty_row);
    REQUIRE(result.find("(0 rows)") != std::string::npos);
}
