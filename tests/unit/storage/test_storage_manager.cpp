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
    StorageManager sm(td.path);
    Catalog catalog;

    REQUIRE(sm.load(catalog));
    REQUIRE(catalog.tableCount() == 0);
}

TEST_CASE("StorageManager - create table persists schema to disk", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    // Session 1: create table
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("users", schema));
        REQUIRE(fs::exists(td.path + "/catalog.meta"));
        REQUIRE(fs::exists(td.path + "/users.db"));
    }

    // Session 2: reload and verify schema survived
    {
        StorageManager sm(td.path);
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

TEST_CASE("StorageManager - insert rows persist to disk", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    // Session 1: insert some rows
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("users", schema));

        REQUIRE(sm.appendRow("users", makeRow(1, "Alice"), schema));
        REQUIRE(sm.appendRow("users", makeRow(2, "Bob"),   schema));
        REQUIRE(sm.appendRow("users", makeRow(3, "Carol"), schema));
    }

    // Session 2: reload and verify rows survived
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.hasTable("users"));

        const Table* table = catalog.getTable("users");
        REQUIRE(table->rowCount() == 3);

        REQUIRE(table->get(0).get(0).asInt32() == 1);
        REQUIRE(table->get(0).get(1).asString() == "Alice");
        REQUIRE(table->get(1).get(0).asInt32() == 2);
        REQUIRE(table->get(1).get(1).asString() == "Bob");
        REQUIRE(table->get(2).get(0).asInt32() == 3);
        REQUIRE(table->get(2).get(1).asString() == "Carol");
    }
}

TEST_CASE("StorageManager - multiple tables persist independently", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("employees", schema));
        REQUIRE(sm.onCreateTable("departments", schema));

        REQUIRE(sm.appendRow("employees",   makeRow(10, "Dave"), schema));
        REQUIRE(sm.appendRow("departments", makeRow(99, "Engineering"), schema));
    }

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.tableCount() == 2);
        REQUIRE(catalog.hasTable("employees"));
        REQUIRE(catalog.hasTable("departments"));

        REQUIRE(catalog.getTable("employees")->rowCount() == 1);
        REQUIRE(catalog.getTable("employees")->get(0).get(1).asString() == "Dave");

        REQUIRE(catalog.getTable("departments")->rowCount() == 1);
        REQUIRE(catalog.getTable("departments")->get(0).get(1).asString() == "Engineering");
    }
}

TEST_CASE("StorageManager - drop table removes files and catalog entry", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("tmp", schema));
        REQUIRE(sm.appendRow("tmp", makeRow(1, "row1"), schema));
    }

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.hasTable("tmp"));
        REQUIRE(sm.onDropTable("tmp"));
    }

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(catalog.tableCount() == 0);
        REQUIRE(!fs::exists(td.path + "/tmp.db"));
    }
}

TEST_CASE("StorageManager - checkpoint persists updated rows", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    // Insert 3 rows
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("test", schema));
        REQUIRE(sm.appendRow("test", makeRow(1, "Before"), schema));
        REQUIRE(sm.appendRow("test", makeRow(2, "Before"), schema));
        REQUIRE(sm.appendRow("test", makeRow(3, "Before"), schema));
    }

    // Reload, simulate UPDATE (change name of row 0 in-memory), then checkpoint
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        Table* table = catalog.getTable("test");
        REQUIRE(table != nullptr);

        // Simulate in-memory UPDATE row 0
        table->update(0, makeRow(1, "After"));

        REQUIRE(sm.checkpoint("test", *table));
    }

    // Verify the update survived restart
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        const Table* table = catalog.getTable("test");
        REQUIRE(table->rowCount() == 3);
        REQUIRE(table->get(0).get(1).asString() == "After");
        REQUIRE(table->get(1).get(1).asString() == "Before");
    }
}

TEST_CASE("StorageManager - checkpoint persists deleted rows", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("deltest", schema));
        for (int i = 1; i <= 5; ++i) {
            REQUIRE(sm.appendRow("deltest", makeRow(i, "row" + std::to_string(i)), schema));
        }
    }

    // Reload, delete rows 1 and 3 (0-indexed), checkpoint
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        Table* table = catalog.getTable("deltest");
        REQUIRE(table->rowCount() == 5);

        // Remove indices 1 and 3 (Bob, Dave)
        table->removeBulk({1, 3});
        REQUIRE(table->rowCount() == 3);
        REQUIRE(sm.checkpoint("deltest", *table));
    }

    // Verify 3 rows remain after restart
    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        const Table* table = catalog.getTable("deltest");
        REQUIRE(table->rowCount() == 3);
        REQUIRE(table->get(0).get(0).asInt32() == 1);
        REQUIRE(table->get(1).get(0).asInt32() == 3);
        REQUIRE(table->get(2).get(0).asInt32() == 5);
    }
}

TEST_CASE("StorageManager - large insert spans multiple pages", "[storage_manager]") {
    TempDir td;
    Schema schema = makeSchema();

    const int ROW_COUNT = 200;

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("big", schema));
        for (int i = 0; i < ROW_COUNT; ++i) {
            REQUIRE(sm.appendRow("big", makeRow(i, "value_" + std::to_string(i)), schema));
        }
    }

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        const Table* table = catalog.getTable("big");
        REQUIRE(table->rowCount() == ROW_COUNT);
        REQUIRE(table->get(0).get(0).asInt32() == 0);
        REQUIRE(table->get(ROW_COUNT - 1).get(0).asInt32() == ROW_COUNT - 1);
    }
}

TEST_CASE("StorageManager - row serializer handles all types", "[storage_manager]") {
    TempDir td;
    Schema schema({
        ColumnSchema("i",  LogicalType(LogicalTypeId::INTEGER),  true),
        ColumnSchema("bi", LogicalType(LogicalTypeId::BIGINT),   true),
        ColumnSchema("f",  LogicalType(LogicalTypeId::FLOAT),    true),
        ColumnSchema("d",  LogicalType(LogicalTypeId::DOUBLE),   true),
        ColumnSchema("b",  LogicalType(LogicalTypeId::BOOLEAN),  true),
        ColumnSchema("s",  LogicalType(LogicalTypeId::VARCHAR),  true),
        ColumnSchema("n",  LogicalType(LogicalTypeId::INTEGER),  true),  // NULL column
    });

    Row row({
        Value::integer(-42),
        Value::bigint(9876543210LL),
        Value::Float(3.14f),
        Value::Double(2.718281828),
        Value::boolean(true),
        Value::varchar("hello"),
        Value::null(),
    });

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        REQUIRE(sm.onCreateTable("types", schema));
        REQUIRE(sm.appendRow("types", row, schema));
    }

    {
        StorageManager sm(td.path);
        Catalog catalog;
        REQUIRE(sm.load(catalog));
        const Table* table = catalog.getTable("types");
        REQUIRE(table->rowCount() == 1);

        const Row& loaded = table->get(0);
        REQUIRE(loaded.get(0).asInt32()  == -42);
        REQUIRE(loaded.get(1).asInt64()  == 9876543210LL);
        REQUIRE(loaded.get(2).asFloat()  == 3.14f);
        REQUIRE(loaded.get(3).asDouble() == 2.718281828);
        REQUIRE(loaded.get(4).asBool()   == true);
        REQUIRE(loaded.get(5).asString() == "hello");
        REQUIRE(loaded.get(6).isNull());
    }
}
