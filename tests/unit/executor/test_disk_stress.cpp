#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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
#include "storage/row.h"
#include "storage/schema.h"
#include "storage/storage_manager.h"
#include "storage/table_iterator.h"
#include "storage/tid.h"

using namespace seeddb;
using Catch::Approx;
namespace fs = std::filesystem;

// =============================================================================
// Stress test fixture — 10-frame buffer pool
// =============================================================================

struct StressFixture {
    std::string dir;
    Config config;
    std::unique_ptr<StorageManager> storage_mgr;
    Catalog catalog;
    std::unique_ptr<Executor> executor;

    static int counter_;

    StressFixture()
        : dir(fs::temp_directory_path().string() + "/seeddb_stress_" +
              std::to_string(++counter_))
        , config()
    {
        fs::create_directories(dir);
        config.set("buffer_pool_size", "10");
        storage_mgr = std::make_unique<StorageManager>(dir, config);
        storage_mgr->load(catalog);
        executor = std::make_unique<Executor>(catalog, storage_mgr.get());
    }

    ~StressFixture() {
        executor.reset();
        storage_mgr.reset();
        fs::remove_all(dir);
    }

    /// Helper: create the test table (id INT, name VARCHAR, score DOUBLE)
    void createTestTable(const std::string& table_name = "stress") {
        auto create = std::make_unique<parser::CreateTableStmt>(table_name);
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "id", parser::DataTypeInfo(parser::DataType::INT)));
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "score", parser::DataTypeInfo(parser::DataType::DOUBLE)));
        auto result = executor->execute(*create);
        REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    }

    /// Helper: insert a row via executor
    void insertRow(const std::string& table, int id,
                   const std::string& name, double score) {
        auto insert = std::make_unique<parser::InsertStmt>(table);
        insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(id)));
        insert->addValues(std::make_unique<parser::LiteralExpr>(name));
        insert->addValues(std::make_unique<parser::LiteralExpr>(score));
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

int StressFixture::counter_ = 0;

// =============================================================================
// Integration Milestone — Section 8.3
// =============================================================================

TEST_CASE("Stress: insert 10,000 rows with 10-frame buffer pool",
          "[stress][integration]") {
    StressFixture fix;
    fix.createTestTable();

    const int ROW_COUNT = 10000;

    // Insert 10,000 rows via executor
    for (int i = 0; i < ROW_COUNT; ++i) {
        fix.insertRow("stress", i,
                      "user_" + std::to_string(i),
                      static_cast<double>(i) * 1.5);
    }

    // Verify all rows are readable
    REQUIRE(fix.countRows("stress") == static_cast<size_t>(ROW_COUNT));

    // Verify data integrity via executor SELECT
    auto select = std::make_unique<parser::SelectStmt>();
    select->setSelectAll(true);
    select->setFromTable(std::make_unique<parser::TableRef>("stress"));

    REQUIRE(fix.executor->prepareSelect(*select));
    int count = 0;
    int first_id = -1;
    double first_score = 0;
    while (fix.executor->hasNext()) {
        auto r = fix.executor->next();
        int id = r.row().get(0).asInt32();
        double score = r.row().get(2).asDouble();
        if (count == 0) {
            first_id = id;
            first_score = score;
        }
        count++;
    }
    REQUIRE(count == ROW_COUNT);
    REQUIRE(first_id >= 0);
    // Verify score formula for first row
    REQUIRE(first_score == Approx(first_id * 1.5));
    fix.executor->resetQuery();

    // Verify multi-page (>10 pages needed for 10K rows)
    REQUIRE(fix.storage_mgr->pageCount("stress") > 10);
}

TEST_CASE("Stress: UPDATE correctness on large dataset",
          "[stress][integration]") {
    StressFixture fix;
    fix.createTestTable();

    const int ROW_COUNT = 5000;

    // Insert rows
    for (int i = 0; i < ROW_COUNT; ++i) {
        fix.insertRow("stress", i, "original", static_cast<double>(i));
    }
    REQUIRE(fix.countRows("stress") == static_cast<size_t>(ROW_COUNT));

    // Update rows where id < ROW_COUNT/2 (half the rows)
    {
        auto update = std::make_unique<parser::UpdateStmt>("stress");
        update->addAssignment("name",
            std::make_unique<parser::LiteralExpr>(std::string("updated")));

        auto where = std::make_unique<parser::BinaryExpr>(
            "<",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(ROW_COUNT / 2)));
        update->setWhere(std::move(where));

        auto result = fix.executor->execute(*update);
        REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    }

    // Verify: count updated vs original rows via storage iterator
    auto iter = fix.storage_mgr->createIterator("stress");
    REQUIRE(iter != nullptr);
    int updated = 0, original = 0;
    while (iter->next()) {
        std::string name = iter->currentRow().get(1).asString();
        if (name == "updated") updated++;
        else if (name == "original") original++;
    }
    REQUIRE(updated == ROW_COUNT / 2);
    REQUIRE(original == ROW_COUNT / 2);
}

TEST_CASE("Stress: DELETE correctness on large dataset",
          "[stress][integration]") {
    StressFixture fix;
    fix.createTestTable();

    const int ROW_COUNT = 5000;

    // Insert rows
    for (int i = 0; i < ROW_COUNT; ++i) {
        fix.insertRow("stress", i, "row", static_cast<double>(i));
    }
    REQUIRE(fix.countRows("stress") == static_cast<size_t>(ROW_COUNT));

    // Delete rows where id >= ROW_COUNT/2
    {
        auto del = std::make_unique<parser::DeleteStmt>("stress");
        auto where = std::make_unique<parser::BinaryExpr>(
            ">=",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(ROW_COUNT / 2)));
        del->setWhere(std::move(where));
        auto result = fix.executor->execute(*del);
        REQUIRE(result.status() == ExecutionResult::Status::EMPTY);
    }

    // Verify: ROW_COUNT/2 rows remain
    auto iter = fix.storage_mgr->createIterator("stress");
    REQUIRE(iter != nullptr);
    int count = 0;
    int max_id = -1;
    while (iter->next()) {
        int id = iter->currentRow().get(0).asInt32();
        REQUIRE(id < ROW_COUNT / 2);
        if (id > max_id) max_id = id;
        count++;
    }
    REQUIRE(count == ROW_COUNT / 2);
    REQUIRE(max_id == ROW_COUNT / 2 - 1);
}

TEST_CASE("Stress: data survives restart with small buffer pool",
          "[stress][integration]") {
    static int sub_counter = 0;
    std::string dir = fs::temp_directory_path().string() + "/seeddb_restart_" +
                      std::to_string(++sub_counter);
    fs::create_directories(dir);

    Config config;
    config.set("buffer_pool_size", "10");

    const int ROW_COUNT = 2000;
    const std::string table_name = "persist";

    // Session 1: Insert rows via executor
    {
        StorageManager sm(dir, config);
        Catalog cat;
        sm.load(cat);
        Executor exec(cat, &sm);

        auto create = std::make_unique<parser::CreateTableStmt>(table_name);
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "id", parser::DataTypeInfo(parser::DataType::INT)));
        create->addColumn(std::make_unique<parser::ColumnDef>(
            "data", parser::DataTypeInfo(parser::DataType::VARCHAR)));
        REQUIRE(exec.execute(*create).status() == ExecutionResult::Status::EMPTY);

        for (int i = 0; i < ROW_COUNT; ++i) {
            auto insert = std::make_unique<parser::InsertStmt>(table_name);
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
        select->setFromTable(std::make_unique<parser::TableRef>(table_name));

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

TEST_CASE("Stress: buffer pool size remains constant",
          "[stress][integration]") {
    StressFixture fix;
    fix.createTestTable();

    // Insert 3000 rows — should need >10 pages with ~30 bytes per row
    for (int i = 0; i < 3000; ++i) {
        fix.insertRow("stress", i, "row", static_cast<double>(i));
    }

    // The buffer pool config should still be 10 frames
    REQUIRE(fix.config.buffer_pool_size() == 10);

    // The table should have more than 10 pages (proving eviction occurred)
    REQUIRE(fix.storage_mgr->pageCount("stress") > 10);

    // And all rows are still correct
    REQUIRE(fix.countRows("stress") == 3000);
}
