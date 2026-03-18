#include <catch2/catch_test_macros.hpp>

#include "executor/executor.h"
#include "common/error.h"
#include "common/value.h"
#include "storage/row.h"
#include "storage/catalog.h"
#include "parser/ast.h"

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

// =============================================================================
// Executor DDL Tests
// =============================================================================

TEST_CASE("Executor: CREATE TABLE creates table in catalog", "[executor][ddl]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create a CREATE TABLE statement
    auto stmt = std::make_unique<parser::CreateTableStmt>("users");
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR, 100)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "balance", parser::DataTypeInfo(parser::DataType::DOUBLE)));

    ExecutionResult result = executor.execute(*stmt);

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    REQUIRE(catalog.hasTable("users"));

    // Verify schema
    const Table* table = catalog.getTable("users");
    REQUIRE(table != nullptr);
    REQUIRE(table->schema().columnCount() == 3);
    REQUIRE(table->schema().column(0).name() == "id");
    REQUIRE(table->schema().column(0).type().id() == LogicalTypeId::INTEGER);
    REQUIRE(table->schema().column(1).name() == "name");
    REQUIRE(table->schema().column(1).type().id() == LogicalTypeId::VARCHAR);
    REQUIRE(table->schema().column(2).name() == "balance");
    REQUIRE(table->schema().column(2).type().id() == LogicalTypeId::DOUBLE);
}

TEST_CASE("Executor: CREATE TABLE fails on duplicate", "[executor][ddl]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create first table
    auto stmt1 = std::make_unique<parser::CreateTableStmt>("users");
    stmt1->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));

    ExecutionResult result1 = executor.execute(*stmt1);
    REQUIRE(result1.status() == ExecutionResult::Status::EMPTY);
    REQUIRE(catalog.hasTable("users"));

    // Try to create duplicate table
    auto stmt2 = std::make_unique<parser::CreateTableStmt>("users");
    stmt2->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::BIGINT)));

    ExecutionResult result2 = executor.execute(*stmt2);
    REQUIRE(result2.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result2.errorCode() == ErrorCode::DUPLICATE_TABLE);
    REQUIRE(result2.errorMessage().find("users") != std::string::npos);
}

TEST_CASE("Executor: DROP TABLE removes table", "[executor][ddl]") {
    Catalog catalog;
    Executor executor(catalog);

    // First create a table
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("temp_table");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);
    REQUIRE(catalog.hasTable("temp_table"));

    // Drop the table
    auto drop_stmt = std::make_unique<parser::DropTableStmt>("temp_table", false);
    ExecutionResult result = executor.execute(*drop_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    REQUIRE_FALSE(catalog.hasTable("temp_table"));
}

TEST_CASE("Executor: DROP TABLE fails if not exists", "[executor][ddl]") {
    Catalog catalog;
    Executor executor(catalog);

    // Try to drop a non-existent table (without IF EXISTS)
    auto drop_stmt = std::make_unique<parser::DropTableStmt>("nonexistent", false);
    ExecutionResult result = executor.execute(*drop_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::TABLE_NOT_FOUND);
    REQUIRE(result.errorMessage().find("nonexistent") != std::string::npos);
}

TEST_CASE("Executor: DROP TABLE IF EXISTS succeeds even if not exists", "[executor][ddl]") {
    Catalog catalog;
    Executor executor(catalog);

    // Try to drop a non-existent table (with IF EXISTS)
    auto drop_stmt = std::make_unique<parser::DropTableStmt>("nonexistent", true);
    ExecutionResult result = executor.execute(*drop_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
}

TEST_CASE("Executor: toLogicalType maps all data types", "[executor][ddl]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table with all data types
    auto stmt = std::make_unique<parser::CreateTableStmt>("all_types");
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_int", parser::DataTypeInfo(parser::DataType::INT)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_bigint", parser::DataTypeInfo(parser::DataType::BIGINT)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_float", parser::DataTypeInfo(parser::DataType::FLOAT)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_double", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_varchar", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_text", parser::DataTypeInfo(parser::DataType::TEXT)));
    stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_bool", parser::DataTypeInfo(parser::DataType::BOOLEAN)));

    ExecutionResult result = executor.execute(*stmt);
    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);

    const Table* table = catalog.getTable("all_types");
    REQUIRE(table != nullptr);
    REQUIRE(table->schema().columnCount() == 7);

    REQUIRE(table->schema().column(0).type().id() == LogicalTypeId::INTEGER);
    REQUIRE(table->schema().column(1).type().id() == LogicalTypeId::BIGINT);
    REQUIRE(table->schema().column(2).type().id() == LogicalTypeId::FLOAT);
    REQUIRE(table->schema().column(3).type().id() == LogicalTypeId::DOUBLE);
    REQUIRE(table->schema().column(4).type().id() == LogicalTypeId::VARCHAR);
    REQUIRE(table->schema().column(5).type().id() == LogicalTypeId::VARCHAR);
    REQUIRE(table->schema().column(6).type().id() == LogicalTypeId::BOOLEAN);
}
