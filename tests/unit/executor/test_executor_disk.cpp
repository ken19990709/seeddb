#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "common/config.h"
#include "common/error.h"
#include "common/value.h"
#include "executor/executor.h"
#include "parser/ast.h"
#include "storage/catalog.h"
#include "storage/page.h"       // for PAGE_SIZE
#include "storage/row.h"
#include "storage/schema.h"
#include "storage/storage_manager.h"
#include "storage/table_iterator.h"
#include "storage/tid.h"

using namespace seeddb;
namespace fs = std::filesystem;

// =============================================================================
// Disk-based executor fixture
// =============================================================================

struct ExecutorDiskFixture {
    std::string dir;
    Config config;
    std::unique_ptr<StorageManager> storage_mgr;
    Catalog catalog;
    std::unique_ptr<Executor> executor;

    static int counter_;

    ExecutorDiskFixture()
        : dir(fs::temp_directory_path().string() + "/seeddb_exec_disk_" +
              std::to_string(++counter_))
        , config()
        , storage_mgr(nullptr)
        , catalog()
        , executor(nullptr)
    {
        fs::create_directories(dir);
        config.set("buffer_pool_size", "10");
        storage_mgr = std::make_unique<StorageManager>(dir, config);
        storage_mgr->load(catalog);
        executor = std::make_unique<Executor>(catalog, storage_mgr.get());
    }

    ~ExecutorDiskFixture() {
        executor.reset();
        storage_mgr.reset();
        fs::remove_all(dir);
    }

    /// Helper: create a two-column table (id INT, name VARCHAR)
    void createSimpleTable(const std::string& name) {
        auto create = std::make_unique<parser::CreateTableStmt>(name);
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "id", parser::DataTypeInfo(parser::DataType::INT)));
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
        auto result = executor->execute(*create);
        REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    }

    /// Helper: insert a row via executor (id INT, name VARCHAR)
    void insertRow(const std::string& table, int id, const std::string& name) {
        auto insert = std::make_unique<parser::InsertStmt>(table);
        insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(id)));
        insert->addValues(std::make_unique<parser::LiteralExpr>(name));
        auto result = executor->execute(*insert);
        REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    }

    /// Helper: count rows via iterator
    size_t countRows(const std::string& table) {
        auto iter = storage_mgr->createIterator(table);
        if (!iter) return 0;
        size_t n = 0;
        while (iter->next()) n++;
        return n;
    }
};

int ExecutorDiskFixture::counter_ = 0;

// =============================================================================
// Section 7 — Executor error propagation tests
// =============================================================================

TEST_CASE("Executor: INSERT oversized row returns error", "[executor][error]") {
    ExecutorDiskFixture fix;

    // Create table with a single VARCHAR column
    auto create = std::make_unique<parser::CreateTableStmt>("bigdata");
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "data", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    REQUIRE(fix.executor->execute(*create).status() == ExecutionResult::Status::EMPTY);

    // Try to insert a row larger than PAGE_SIZE
    std::string huge(PAGE_SIZE + 100, 'X');
    auto insert = std::make_unique<parser::InsertStmt>("bigdata");
    insert->addValues(std::make_unique<parser::LiteralExpr>(huge));

    auto result = fix.executor->execute(*insert);
    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::INTERNAL_ERROR);
}

TEST_CASE("Executor: SELECT from dropped table returns error", "[executor][error]") {
    ExecutorDiskFixture fix;

    // Create and immediately drop table
    fix.createSimpleTable("temp");
    auto drop = std::make_unique<parser::DropTableStmt>("temp", false);
    fix.executor->execute(*drop);

    // SELECT should fail
    auto select = std::make_unique<parser::SelectStmt>();
    select->setSelectAll(true);
    select->setFromTable(std::make_unique<parser::TableRef>("temp"));

    auto result = fix.executor->execute(*select);
    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::TABLE_NOT_FOUND);
}

TEST_CASE("Executor: UPDATE on dropped table returns error", "[executor][error]") {
    ExecutorDiskFixture fix;

    fix.createSimpleTable("upd_err");
    auto drop = std::make_unique<parser::DropTableStmt>("upd_err", false);
    fix.executor->execute(*drop);

    auto update = std::make_unique<parser::UpdateStmt>("upd_err");
    update->addAssignment("name",
        std::make_unique<parser::LiteralExpr>(int64_t(42)));
    auto result = fix.executor->execute(*update);
    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::TABLE_NOT_FOUND);
}

TEST_CASE("Executor: DELETE on dropped table returns error", "[executor][error]") {
    ExecutorDiskFixture fix;

    fix.createSimpleTable("del_err");
    auto drop = std::make_unique<parser::DropTableStmt>("del_err", false);
    fix.executor->execute(*drop);

    auto del = std::make_unique<parser::DeleteStmt>("del_err");
    auto result = fix.executor->execute(*del);
    REQUIRE(result.status() == ExecutionResult::Status::ERROR);
    REQUIRE(result.errorCode() == ErrorCode::TABLE_NOT_FOUND);
}

TEST_CASE("Executor: INSERT-SELECT-UPDATE-DELETE sequential cycle",
          "[executor][integration]") {
    ExecutorDiskFixture fix;

    // Create table
    auto create = std::make_unique<parser::CreateTableStmt>("seq");
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    REQUIRE(fix.executor->execute(*create).status() == ExecutionResult::Status::EMPTY);

    // INSERT 10 rows
    for (int i = 0; i < 10; ++i) {
        auto insert = std::make_unique<parser::InsertStmt>("seq");
        insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i)));
        insert->addValues(std::make_unique<parser::LiteralExpr>("val" + std::to_string(i)));
        REQUIRE(fix.executor->execute(*insert).status() == ExecutionResult::Status::EMPTY);
    }

    // SELECT all — verify 10 rows
    {
        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("seq"));
        REQUIRE(fix.executor->prepareSelect(*select));
        int count = 0;
        while (fix.executor->hasNext()) {
            fix.executor->next();
            count++;
        }
        REQUIRE(count == 10);
        fix.executor->resetQuery();
    }

    // UPDATE rows where id < 5
    {
        auto update = std::make_unique<parser::UpdateStmt>("seq");
        update->addAssignment("name",
            std::make_unique<parser::LiteralExpr>(std::string("updated")));
        auto where = std::make_unique<parser::BinaryExpr>(
            "<",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(5)));
        update->setWhere(std::move(where));
        REQUIRE(fix.executor->execute(*update).status() == ExecutionResult::Status::EMPTY);
    }

    // Verify update — 5 rows have "updated", 5 have "valN"
    {
        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("seq"));
        REQUIRE(fix.executor->prepareSelect(*select));
        int updated_count = 0;
        int original_count = 0;
        while (fix.executor->hasNext()) {
            auto r = fix.executor->next();
            std::string name = r.row().get(1).asString();
            if (name == "updated") updated_count++;
            else original_count++;
        }
        REQUIRE(updated_count == 5);
        REQUIRE(original_count == 5);
        fix.executor->resetQuery();
    }

    // DELETE rows where id >= 5
    {
        auto del = std::make_unique<parser::DeleteStmt>("seq");
        auto where = std::make_unique<parser::BinaryExpr>(
            ">=",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(5)));
        del->setWhere(std::move(where));
        REQUIRE(fix.executor->execute(*del).status() == ExecutionResult::Status::EMPTY);
    }

    // Verify — 5 rows remain (id 0-4)
    {
        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("seq"));
        REQUIRE(fix.executor->prepareSelect(*select));
        int count = 0;
        while (fix.executor->hasNext()) {
            auto r = fix.executor->next();
            REQUIRE(r.row().get(0).asInt32() < 5);
            REQUIRE(r.row().get(1).asString() == "updated");
            count++;
        }
        REQUIRE(count == 5);
        fix.executor->resetQuery();
    }
}

// =============================================================================
// Executor with disk persistence — DDL/DML persistence across sessions
// =============================================================================

TEST_CASE("Executor: data persists across sessions", "[executor][integration]") {
    static int sub_counter = 0;
    std::string dir = fs::temp_directory_path().string() + "/seeddb_exec_persist_" +
                      std::to_string(++sub_counter);
    fs::create_directories(dir);

    Config config;
    config.set("buffer_pool_size", "10");

    const int ROW_COUNT = 50;

    // Session 1: Create table and insert rows
    {
        StorageManager sm(dir, config);
        Catalog cat;
        sm.load(cat);
        Executor exec(cat, &sm);

        auto create = std::make_unique<parser::CreateTableStmt>("persist");
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "id", parser::DataTypeInfo(parser::DataType::INT)));
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "data", parser::DataTypeInfo(parser::DataType::VARCHAR)));
        REQUIRE(exec.execute(*create).status() == ExecutionResult::Status::EMPTY);

        for (int i = 0; i < ROW_COUNT; ++i) {
            auto insert = std::make_unique<parser::InsertStmt>("persist");
            insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i)));
            insert->addValues(std::make_unique<parser::LiteralExpr>("data_" + std::to_string(i)));
            REQUIRE(exec.execute(*insert).status() == ExecutionResult::Status::EMPTY);
        }
        // ~sm flushes dirty pages
    }

    // Session 2: Read back and verify
    {
        StorageManager sm(dir, config);
        Catalog cat;
        sm.load(cat);
        Executor exec(cat, &sm);

        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("persist"));

        REQUIRE(exec.prepareSelect(*select));
        int count = 0;
        while (exec.hasNext()) {
            auto r = exec.next();
            int id = r.row().get(0).asInt32();
            std::string data = r.row().get(1).asString();
            REQUIRE(data == "data_" + std::to_string(id));
            count++;
        }
        REQUIRE(count == ROW_COUNT);
        exec.resetQuery();
    }

    fs::remove_all(dir);
}
