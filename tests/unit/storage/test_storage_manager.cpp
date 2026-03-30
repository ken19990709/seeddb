#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>

#include <filesystem>
#include <string>

#include "storage/catalog.h"
#include "storage/row.h"
#include "storage/schema.h"
#include "storage/storage_manager.h"
#include "storage/table.h"
#include "common/value.h"
#include "common/config.h"
#include "storage/tid.h"
#include "storage/table_iterator.h"

namespace fs = std::filesystem;
using namespace seeddb;

// =============================================================================
// Test fixture – creates a unique temporary directory per test case
// =============================================================================

struct TempDir {
    std::string path;

    TempDir() {
        path = fs::temp_directory_path().string() + "/seeddb_sm_test_" +
               std::to_string(reinterpret_cast<uintptr_t>(this));
        fs::create_directories(path);
    }

    ~TempDir() {
        fs::remove_all(path);
    }
};

struct TempDirCfg {
    std::string path;
    seeddb::Config config;

    TempDirCfg() {
        path = fs::temp_directory_path().string() + "/seeddb_sm_new_" +
               std::to_string(reinterpret_cast<uintptr_t>(this));
        fs::create_directories(path);
        config.set("buffer_pool_size", "10");
    }

    ~TempDirCfg() {
        fs::remove_all(path);
    }
};

// ---------------------------------------------------------------------------
// Helper – build a simple two-column schema: id INTEGER, name VARCHAR
// ---------------------------------------------------------------------------
static Schema makeSchema() {
    return Schema({
        ColumnSchema("id",   LogicalType(LogicalTypeId::INTEGER), false),
        ColumnSchema("name", LogicalType(LogicalTypeId::VARCHAR), true),
    });
}

// ---------------------------------------------------------------------------
// Helper – build a Row matching makeSchema()
// ---------------------------------------------------------------------------
static Row makeRow(int id, const std::string& name) {
    return Row({Value::integer(id), Value::varchar(name)});
}

// =============================================================================
// Test cases
// =============================================================================

TEST_CASE("StorageManager - empty database loads successfully", "[storage_manager]") {
    TempDir td;
    StorageManager sm(td.path, Config{});
    Catalog catalog;

    REQUIRE(sm.load(catalog));
    REQUIRE(catalog.tableCount() == 0);
}

TEST_CASE("StorageManager - create table persists schema to disk", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    // Session 1: create table
    {
        StorageManager sm(td.path, Config{});
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("users", schema));
        REQUIRE(fs::exists(td.path + "/catalog.meta"));
        REQUIRE(fs::exists(td.path + "/users.db"));
    }

    // Session 2: reload and verify schema survived
    {
        StorageManager sm(td.path, Config{});
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.tableCount() == 1);
        REQUIRE(catalog.hasTable("users"));

        const Schema& loaded = catalog.getTable("users")->schema();
        REQUIRE(loaded.columnCount() == 2);
        REQUIRE(loaded.column(0).name() == "id");
        REQUIRE(loaded.column(1).name() == "name");
        REQUIRE(loaded.column(0).type().id() == LogicalTypeId::INTEGER);
        REQUIRE(loaded.column(1).type().id() == LogicalTypeId::VARCHAR);
        REQUIRE(!loaded.column(0).isNullable());
        REQUIRE(loaded.column(1).isNullable());
    }
}

TEST_CASE("StorageManager - drop table removes files and catalog entry", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    {
        StorageManager sm(td.path, Config{});
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("tmp", schema));
        REQUIRE(sm.insertRow("tmp", makeRow(1, "row1"), schema));
    }

    {
        StorageManager sm(td.path, Config{});
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.hasTable("tmp"));
        REQUIRE(sm.onDropTable("tmp"));
    }

    {
        StorageManager sm(td.path, Config{});
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.tableCount() == 0);
        REQUIRE(!fs::exists(td.path + "/tmp.db"));
    }
}

// =============================================================================
// New disk-based API tests
// =============================================================================

TEST_CASE("StorageManager - insertRow persists via BufferPool", "[storage_manager]") {
    TempDirCfg td;
    Schema schema = makeSchema();

    // Session 1: insert rows via new API
    {
        StorageManager sm(td.path, td.config);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("users", schema));

        REQUIRE(sm.insertRow("users", makeRow(1, "Alice"), schema));
        REQUIRE(sm.insertRow("users", makeRow(2, "Bob"),   schema));
        REQUIRE(sm.insertRow("users", makeRow(3, "Carol"), schema));
        // ~sm flushes dirty pages
    }

    // Session 2: read back via iterator
    {
        StorageManager sm(td.path, td.config);
        Catalog catalog;
        REQUIRE(sm.load(catalog));

        auto iter = sm.createIterator("users");
        REQUIRE(iter != nullptr);

        std::vector<Row> rows;
        while (iter->next()) {
            rows.push_back(Row(iter->currentRow()));
        }
        REQUIRE(rows.size() == 3);
        REQUIRE(rows[0].get(0).asInt32() == 1);
        REQUIRE(rows[0].get(1).asString() == "Alice");
        REQUIRE(rows[2].get(0).asInt32() == 3);
    }
}

TEST_CASE("StorageManager - deleteRow removes row", "[storage_manager]") {
    TempDirCfg td;
    Schema schema = makeSchema();

    {
        StorageManager sm(td.path, td.config);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("items", schema));

        REQUIRE(sm.insertRow("items", makeRow(10, "A"), schema));
        REQUIRE(sm.insertRow("items", makeRow(20, "B"), schema));
        REQUIRE(sm.insertRow("items", makeRow(30, "C"), schema));

        // Collect TIDs
        auto iter = sm.createIterator("items");
        REQUIRE(iter != nullptr);
        std::vector<TID> tids;
        while (iter->next()) {
            tids.push_back(iter->currentTID());
        }
        REQUIRE(tids.size() == 3);

        // Delete row with id=20 (TID at index 1)
        REQUIRE(sm.deleteRow(tids[1]));
    }

    // Verify deletion
    {
        StorageManager sm(td.path, td.config);
        Catalog catalog;
        REQUIRE(sm.load(catalog));

        auto iter = sm.createIterator("items");
        std::vector<int> ids;
        while (iter->next()) {
            ids.push_back(iter->currentRow().get(0).asInt32());
        }
        REQUIRE(ids.size() == 2);
        REQUIRE(ids[0] == 10);
        REQUIRE(ids[1] == 30);
    }
}

TEST_CASE("StorageManager - updateRow modifies row", "[storage_manager]") {
    TempDirCfg td;
    Schema schema = makeSchema();

    {
        StorageManager sm(td.path, td.config);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("upd", schema));

        REQUIRE(sm.insertRow("upd", makeRow(1, "old"), schema));

        // Get TID
        auto iter = sm.createIterator("upd");
        REQUIRE(iter->next());
        TID tid = iter->currentTID();
        REQUIRE(tid.isValid());

        // Update
        REQUIRE(sm.updateRow(tid, makeRow(1, "new"), schema));
    }

    {
        StorageManager sm(td.path, td.config);
        Catalog catalog;
        REQUIRE(sm.load(catalog));

        auto iter = sm.createIterator("upd");
        REQUIRE(iter->next());
        REQUIRE(iter->currentRow().get(1).asString() == "new");
        REQUIRE_FALSE(iter->next());
    }
}

TEST_CASE("StorageManager - pageCount returns correct count", "[storage_manager]") {
    TempDirCfg td;
    Schema schema = makeSchema();

    StorageManager sm(td.path, td.config);
    Catalog catalog;
    REQUIRE(sm.load(catalog));
    REQUIRE(sm.onCreateTable("pg", schema));

    REQUIRE(sm.pageCount("pg") == 0);

    // Insert enough rows to span multiple pages
    for (int i = 0; i < 200; ++i) {
        REQUIRE(sm.insertRow("pg", makeRow(i, "val_" + std::to_string(i)), schema));
    }

    REQUIRE(sm.pageCount("pg") > 1);
    REQUIRE(sm.pageCount("nonexistent") == 0);
}

TEST_CASE("StorageManager - createIterator returns nullptr for missing table", "[storage_manager]") {
    TempDirCfg td;
    StorageManager sm(td.path, td.config);
    Catalog catalog;
    REQUIRE(sm.load(catalog));

    REQUIRE(sm.createIterator("no_such_table") == nullptr);
}

TEST_CASE("StorageManager - insertRow returns false for oversized row", "[storage_manager]") {
    TempDirCfg td;
    Schema schema({ColumnSchema("data", LogicalType(LogicalTypeId::VARCHAR), false)});

    StorageManager sm(td.path, td.config);
    Catalog catalog;
    REQUIRE(sm.load(catalog));
    REQUIRE(sm.onCreateTable("big", schema));

    // Create a row larger than a single page (PAGE_SIZE = 4096)
    std::string huge(PAGE_SIZE + 100, 'x');
    Row oversized({Value::varchar(huge)});

    REQUIRE_FALSE(sm.insertRow("big", oversized, schema));
}
