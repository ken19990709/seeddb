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

// =============================================================================
// Executor INSERT Tests
// =============================================================================

TEST_CASE("Executor: INSERT into table", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table first
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR, 100)));
    executor.execute(*create_stmt);

    // Insert a row
    auto insert_stmt = std::make_unique<parser::InsertStmt>("users");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("Alice")));

    ExecutionResult result = executor.execute(*insert_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);

    // Verify the row was inserted
    const Table* table = catalog.getTable("users");
    REQUIRE(table != nullptr);
    REQUIRE(table->rowCount() == 1);
    REQUIRE(table->get(0).size() == 2);
    REQUIRE(table->get(0).get(0).asInt32() == 1);
    REQUIRE(table->get(0).get(1).asString() == "Alice");
}

TEST_CASE("Executor: INSERT multiple rows", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table first
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);

    // Insert first row
    auto insert1 = std::make_unique<parser::InsertStmt>("users");
    insert1->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));
    insert1->addValues(std::make_unique<parser::LiteralExpr>(std::string("Alice")));
    ExecutionResult result1 = executor.execute(*insert1);
    REQUIRE(result1.status() == ExecutionResult::Status::EMPTY);

    // Insert second row
    auto insert2 = std::make_unique<parser::InsertStmt>("users");
    insert2->addValues(std::make_unique<parser::LiteralExpr>(int64_t(2)));
    insert2->addValues(std::make_unique<parser::LiteralExpr>(std::string("Bob")));
    ExecutionResult result2 = executor.execute(*insert2);
    REQUIRE(result2.status() == ExecutionResult::Status::EMPTY);

    // Insert third row
    auto insert3 = std::make_unique<parser::InsertStmt>("users");
    insert3->addValues(std::make_unique<parser::LiteralExpr>(int64_t(3)));
    insert3->addValues(std::make_unique<parser::LiteralExpr>(std::string("Charlie")));
    ExecutionResult result3 = executor.execute(*insert3);
    REQUIRE(result3.status() == ExecutionResult::Status::EMPTY);

    // Verify all rows were inserted
    const Table* table = catalog.getTable("users");
    REQUIRE(table != nullptr);
    REQUIRE(table->rowCount() == 3);
}

TEST_CASE("Executor: INSERT fails on non-existent table", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Try to insert into non-existent table
    auto insert_stmt = std::make_unique<parser::InsertStmt>("nonexistent");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));

    ExecutionResult result = executor.execute(*insert_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::TABLE_NOT_FOUND);
    REQUIRE(result.errorMessage().find("nonexistent") != std::string::npos);
}

TEST_CASE("Executor: INSERT fails on column count mismatch", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table with 2 columns
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);

    // Try to insert with wrong number of values (1 instead of 2)
    auto insert_stmt = std::make_unique<parser::InsertStmt>("users");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));

    ExecutionResult result = executor.execute(*insert_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::CONSTRAINT_VIOLATION);
    REQUIRE(result.errorMessage().find("Column count mismatch") != std::string::npos);
}

TEST_CASE("Executor: INSERT fails on NOT NULL constraint violation", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table with NOT NULL column
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    auto id_col = std::make_unique<parser::ColumnDef>("id", parser::DataTypeInfo(parser::DataType::INT));
    id_col->setNullable(false);  // NOT NULL
    create_stmt->addColumn(std::move(id_col));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);

    // Try to insert NULL into NOT NULL column
    auto insert_stmt = std::make_unique<parser::InsertStmt>("users");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>());  // NULL
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("Alice")));

    ExecutionResult result = executor.execute(*insert_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::CONSTRAINT_VIOLATION);
    REQUIRE(result.errorMessage().find("schema constraints") != std::string::npos);
}

TEST_CASE("Executor: INSERT with NULL into nullable column", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table with nullable columns (default)
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));  // nullable by default
    executor.execute(*create_stmt);

    // Insert NULL into nullable column
    auto insert_stmt = std::make_unique<parser::InsertStmt>("users");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>());  // NULL

    ExecutionResult result = executor.execute(*insert_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);

    // Verify the row was inserted with NULL
    const Table* table = catalog.getTable("users");
    REQUIRE(table != nullptr);
    REQUIRE(table->rowCount() == 1);
    REQUIRE(table->get(0).get(0).asInt32() == 1);
    REQUIRE(table->get(0).get(1).isNull());
}

TEST_CASE("Executor: INSERT with various data types", "[executor][dml]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table with all data types
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("all_types");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_int", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_bigint", parser::DataTypeInfo(parser::DataType::BIGINT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_float", parser::DataTypeInfo(parser::DataType::FLOAT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_double", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_varchar", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "col_bool", parser::DataTypeInfo(parser::DataType::BOOLEAN)));
    executor.execute(*create_stmt);

    // Insert row with all types
    auto insert_stmt = std::make_unique<parser::InsertStmt>("all_types");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(42)));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(9223372036854775807L)));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(3.14));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(2.718281828459045));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("hello")));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(true));

    ExecutionResult result = executor.execute(*insert_stmt);

    REQUIRE(result.status() == ExecutionResult::Status::EMPTY);

    // Verify the row was inserted
    const Table* table = catalog.getTable("all_types");
    REQUIRE(table != nullptr);
    REQUIRE(table->rowCount() == 1);
    REQUIRE(table->get(0).get(0).asInt32() == 42);
    REQUIRE(table->get(0).get(1).asInt64() == 9223372036854775807L);
    // Float comparison with tolerance
    REQUIRE(std::abs(table->get(0).get(2).asFloat() - 3.14f) < 0.001f);
    REQUIRE(std::abs(table->get(0).get(3).asDouble() - 2.718281828459045) < 0.0001);
    REQUIRE(table->get(0).get(4).asString() == "hello");
    REQUIRE(table->get(0).get(5).asBool() == true);
}

// =============================================================================
// Executor SELECT Tests
// =============================================================================

TEST_CASE("Executor: SELECT * from table with rows", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);

    auto insert1 = std::make_unique<parser::InsertStmt>("users");
    insert1->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));
    insert1->addValues(std::make_unique<parser::LiteralExpr>(std::string("Alice")));
    executor.execute(*insert1);

    auto insert2 = std::make_unique<parser::InsertStmt>("users");
    insert2->addValues(std::make_unique<parser::LiteralExpr>(int64_t(2)));
    insert2->addValues(std::make_unique<parser::LiteralExpr>(std::string("Bob")));
    executor.execute(*insert2);

    // Prepare SELECT statement
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("users"));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE(prepared);
    REQUIRE(executor.hasNext());

    // Iterate and verify rows
    int row_count = 0;
    while (executor.hasNext()) {
        ExecutionResult result = executor.next();
        REQUIRE(result.status() == ExecutionResult::Status::OK);
        REQUIRE(result.hasRow());
        REQUIRE(result.row().size() == 2);
        row_count++;
    }

    REQUIRE(row_count == 2);
    REQUIRE_FALSE(executor.hasNext());
}

TEST_CASE("Executor: SELECT from empty table", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create empty table
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("empty_table");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);

    // Prepare SELECT statement
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("empty_table"));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE(prepared);
    REQUIRE_FALSE(executor.hasNext());

    // Calling next on empty result should return error
    ExecutionResult result = executor.next();
    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
}

TEST_CASE("Executor: SELECT from non-existent table", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Prepare SELECT statement for non-existent table
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("nonexistent"));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE_FALSE(prepared);
}

TEST_CASE("Executor: SELECT with WHERE clause equality", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);

    auto insert1 = std::make_unique<parser::InsertStmt>("users");
    insert1->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));
    insert1->addValues(std::make_unique<parser::LiteralExpr>(std::string("Alice")));
    executor.execute(*insert1);

    auto insert2 = std::make_unique<parser::InsertStmt>("users");
    insert2->addValues(std::make_unique<parser::LiteralExpr>(int64_t(2)));
    insert2->addValues(std::make_unique<parser::LiteralExpr>(std::string("Bob")));
    executor.execute(*insert2);

    auto insert3 = std::make_unique<parser::InsertStmt>("users");
    insert3->addValues(std::make_unique<parser::LiteralExpr>(int64_t(3)));
    insert3->addValues(std::make_unique<parser::LiteralExpr>(std::string("Charlie")));
    executor.execute(*insert3);

    // SELECT with WHERE id = 2
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("users"));

    auto where_expr = std::make_unique<parser::BinaryExpr>(
        "=",
        std::make_unique<parser::ColumnRef>("id"),
        std::make_unique<parser::LiteralExpr>(int64_t(2))
    );
    select_stmt->setWhere(std::move(where_expr));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE(prepared);
    REQUIRE(executor.hasNext());

    ExecutionResult result = executor.next();
    REQUIRE(result.status() == ExecutionResult::Status::OK);
    REQUIRE(result.row().get(0).asInt32() == 2);
    REQUIRE(result.row().get(1).asString() == "Bob");

    REQUIRE_FALSE(executor.hasNext());  // Only one row should match
}

TEST_CASE("Executor: SELECT with WHERE clause comparison", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("nums");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);

    for (int i = 1; i <= 5; ++i) {
        auto insert_stmt = std::make_unique<parser::InsertStmt>("nums");
        insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i * 10)));
        executor.execute(*insert_stmt);
    }

    // SELECT with WHERE val > 25
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("nums"));

    auto where_expr = std::make_unique<parser::BinaryExpr>(
        ">",
        std::make_unique<parser::ColumnRef>("val"),
        std::make_unique<parser::LiteralExpr>(int64_t(25))
    );
    select_stmt->setWhere(std::move(where_expr));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE(prepared);

    // Should match val = 30, 40, 50
    int count = 0;
    while (executor.hasNext()) {
        ExecutionResult result = executor.next();
        REQUIRE(result.status() == ExecutionResult::Status::OK);
        REQUIRE(result.row().get(0).asInt32() > 25);
        count++;
    }
    REQUIRE(count == 3);
}

TEST_CASE("Executor: SELECT with WHERE clause AND", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("products");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "price", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "qty", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);

    auto insert1 = std::make_unique<parser::InsertStmt>("products");
    insert1->addValues(std::make_unique<parser::LiteralExpr>(int64_t(10)));
    insert1->addValues(std::make_unique<parser::LiteralExpr>(int64_t(5)));
    executor.execute(*insert1);

    auto insert2 = std::make_unique<parser::InsertStmt>("products");
    insert2->addValues(std::make_unique<parser::LiteralExpr>(int64_t(20)));
    insert2->addValues(std::make_unique<parser::LiteralExpr>(int64_t(3)));
    executor.execute(*insert2);

    auto insert3 = std::make_unique<parser::InsertStmt>("products");
    insert3->addValues(std::make_unique<parser::LiteralExpr>(int64_t(15)));
    insert3->addValues(std::make_unique<parser::LiteralExpr>(int64_t(10)));
    executor.execute(*insert3);

    // SELECT with WHERE price > 12 AND qty > 4
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("products"));

    auto and_expr = std::make_unique<parser::BinaryExpr>(
        "AND",
        std::make_unique<parser::BinaryExpr>(
            ">",
            std::make_unique<parser::ColumnRef>("price"),
            std::make_unique<parser::LiteralExpr>(int64_t(12))
        ),
        std::make_unique<parser::BinaryExpr>(
            ">",
            std::make_unique<parser::ColumnRef>("qty"),
            std::make_unique<parser::LiteralExpr>(int64_t(4))
        )
    );
    select_stmt->setWhere(std::move(and_expr));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE(prepared);

    // Only row (15, 10) should match both conditions
    int count = 0;
    while (executor.hasNext()) {
        ExecutionResult result = executor.next();
        REQUIRE(result.status() == ExecutionResult::Status::OK);
        REQUIRE(result.row().get(0).asInt32() == 15);
        REQUIRE(result.row().get(1).asInt32() == 10);
        count++;
    }
    REQUIRE(count == 1);
}

TEST_CASE("Executor: SELECT with WHERE clause no matches", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("nums");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);

    auto insert_stmt = std::make_unique<parser::InsertStmt>("nums");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(10)));
    executor.execute(*insert_stmt);

    // SELECT with WHERE val > 1000 (no matches)
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("nums"));

    auto where_expr = std::make_unique<parser::BinaryExpr>(
        ">",
        std::make_unique<parser::ColumnRef>("val"),
        std::make_unique<parser::LiteralExpr>(int64_t(1000))
    );
    select_stmt->setWhere(std::move(where_expr));

    bool prepared = executor.prepareSelect(*select_stmt);
    REQUIRE(prepared);
    REQUIRE_FALSE(executor.hasNext());  // No rows should match
}

TEST_CASE("Executor: SELECT resetQuery clears state", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);

    auto insert_stmt = std::make_unique<parser::InsertStmt>("users");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(1)));
    executor.execute(*insert_stmt);

    // Prepare and partially iterate
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("users"));

    executor.prepareSelect(*select_stmt);
    REQUIRE(executor.hasNext());
    executor.next();

    // Reset should clear state
    executor.resetQuery();
    REQUIRE_FALSE(executor.hasNext());
}

TEST_CASE("Executor: SELECT with multiple prepare calls", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table and insert rows
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("nums");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);

    for (int i = 1; i <= 3; ++i) {
        auto insert_stmt = std::make_unique<parser::InsertStmt>("nums");
        insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i)));
        executor.execute(*insert_stmt);
    }

    // First SELECT
    auto select1 = std::make_unique<parser::SelectStmt>();
    select1->setSelectAll(true);
    select1->setFromTable(std::make_unique<parser::TableRef>("nums"));
    select1->setWhere(std::make_unique<parser::BinaryExpr>(
        "<",
        std::make_unique<parser::ColumnRef>("val"),
        std::make_unique<parser::LiteralExpr>(int64_t(3))
    ));

    executor.prepareSelect(*select1);
    int count1 = 0;
    while (executor.hasNext()) {
        executor.next();
        count1++;
    }
    REQUIRE(count1 == 2);  // val = 1, 2

    // Second SELECT (should reset state)
    auto select2 = std::make_unique<parser::SelectStmt>();
    select2->setSelectAll(true);
    select2->setFromTable(std::make_unique<parser::TableRef>("nums"));
    select2->setWhere(std::make_unique<parser::BinaryExpr>(
        ">",
        std::make_unique<parser::ColumnRef>("val"),
        std::make_unique<parser::LiteralExpr>(int64_t(1))
    ));

    executor.prepareSelect(*select2);
    int count2 = 0;
    while (executor.hasNext()) {
        executor.next();
        count2++;
    }
    REQUIRE(count2 == 2);  // val = 2, 3
}

TEST_CASE("Executor: SELECT verify row data integrity", "[executor][dml][select]") {
    Catalog catalog;
    Executor executor(catalog);

    // Create table with multiple types
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("mixed");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "active", parser::DataTypeInfo(parser::DataType::BOOLEAN)));
    executor.execute(*create_stmt);

    // Insert specific values
    auto insert_stmt = std::make_unique<parser::InsertStmt>("mixed");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(42)));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("test")));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(true));
    executor.execute(*insert_stmt);

    // SELECT and verify
    auto select_stmt = std::make_unique<parser::SelectStmt>();
    select_stmt->setSelectAll(true);
    select_stmt->setFromTable(std::make_unique<parser::TableRef>("mixed"));

    executor.prepareSelect(*select_stmt);
    REQUIRE(executor.hasNext());

    ExecutionResult result = executor.next();
    REQUIRE(result.status() == ExecutionResult::Status::OK);
    REQUIRE(result.row().size() == 3);
    REQUIRE(result.row().get(0).asInt32() == 42);
    REQUIRE(result.row().get(1).asString() == "test");
    REQUIRE(result.row().get(2).asBool() == true);
}
