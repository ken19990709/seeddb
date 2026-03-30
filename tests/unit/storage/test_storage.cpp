#include <catch2/catch_test_macros.hpp>

#include "storage/catalog.h"
#include "storage/row.h"
#include "storage/schema.h"
#include "storage/table.h"

// =============================================================================
// Row Class Tests
// =============================================================================

TEST_CASE("Row default constructor creates empty row", "[storage][row]") {
    seeddb::Row row;
    REQUIRE(row.empty());
    REQUIRE(row.size() == 0);
}

TEST_CASE("Row constructor from values", "[storage][row]") {
    std::vector<seeddb::Value> values;
    values.push_back(seeddb::Value::integer(42));
    values.push_back(seeddb::Value::varchar("hello"));
    values.push_back(seeddb::Value::boolean(true));

    seeddb::Row row(values);
    REQUIRE(!row.empty());
    REQUIRE(row.size() == 3);
}

TEST_CASE("Row size and empty", "[storage][row]") {
    seeddb::Row empty_row;
    REQUIRE(empty_row.empty());
    REQUIRE(empty_row.size() == 0);

    seeddb::Row row({seeddb::Value::integer(1), seeddb::Value::integer(2)});
    REQUIRE(!row.empty());
    REQUIRE(row.size() == 2);
}

TEST_CASE("Row get const accessor", "[storage][row]") {
    seeddb::Row row({
        seeddb::Value::integer(42),
        seeddb::Value::varchar("test"),
        seeddb::Value::boolean(false)
    });

    const seeddb::Row& const_row = row;
    REQUIRE(const_row.get(0).asInt32() == 42);
    REQUIRE(const_row.get(1).asString() == "test");
    REQUIRE(const_row.get(2).asBool() == false);
}

TEST_CASE("Row get mutable accessor", "[storage][row]") {
    seeddb::Row row({seeddb::Value::integer(10)});
    REQUIRE(row.get(0).asInt32() == 10);

    row.get(0) = seeddb::Value::integer(99);
    REQUIRE(row.get(0).asInt32() == 99);
}

TEST_CASE("Row append", "[storage][row]") {
    seeddb::Row row;
    REQUIRE(row.size() == 0);

    row.append(seeddb::Value::integer(1));
    REQUIRE(row.size() == 1);
    REQUIRE(row.get(0).asInt32() == 1);

    row.append(seeddb::Value::varchar("appended"));
    REQUIRE(row.size() == 2);
    REQUIRE(row.get(1).asString() == "appended");
}

TEST_CASE("Row set", "[storage][row]") {
    seeddb::Row row({
        seeddb::Value::integer(1),
        seeddb::Value::integer(2),
        seeddb::Value::integer(3)
    });

    row.set(1, seeddb::Value::integer(99));
    REQUIRE(row.get(1).asInt32() == 99);
    REQUIRE(row.size() == 3);  // size unchanged
}

TEST_CASE("Row clear", "[storage][row]") {
    seeddb::Row row({seeddb::Value::integer(1), seeddb::Value::integer(2)});
    REQUIRE(row.size() == 2);

    row.clear();
    REQUIRE(row.empty());
    REQUIRE(row.size() == 0);
}

TEST_CASE("Row toString", "[storage][row]") {
    SECTION("Empty row") {
        seeddb::Row row;
        REQUIRE(row.toString() == "()");
    }

    SECTION("Single value") {
        seeddb::Row row({seeddb::Value::integer(42)});
        REQUIRE(row.toString() == "(42)");
    }

    SECTION("Multiple values") {
        seeddb::Row row({
            seeddb::Value::integer(1),
            seeddb::Value::varchar("hello"),
            seeddb::Value::boolean(true)
        });
        REQUIRE(row.toString() == "(1, hello, true)");
    }

    SECTION("NULL value") {
        seeddb::Row row({seeddb::Value::null(), seeddb::Value::integer(10)});
        REQUIRE(row.toString() == "(NULL, 10)");
    }
}

TEST_CASE("Row iterator support", "[storage][row]") {
    seeddb::Row row({
        seeddb::Value::integer(1),
        seeddb::Value::integer(2),
        seeddb::Value::integer(3)
    });

    SECTION("Range-based for loop") {
        int sum = 0;
        for (const auto& value : row) {
            sum += value.asInt32();
        }
        REQUIRE(sum == 6);
    }

    SECTION("Iterator operations") {
        auto it = row.begin();
        REQUIRE(it != row.end());
        REQUIRE(it->asInt32() == 1);

        ++it;
        REQUIRE(it->asInt32() == 2);

        ++it;
        REQUIRE(it->asInt32() == 3);

        ++it;
        REQUIRE(it == row.end());
    }
}

TEST_CASE("Row with various value types", "[storage][row]") {
    seeddb::Row row({
        seeddb::Value::integer(42),
        seeddb::Value::bigint(12345678901234LL),
        seeddb::Value::Float(3.14f),
        seeddb::Value::Double(2.718281828),
        seeddb::Value::varchar("test string"),
        seeddb::Value::boolean(true),
        seeddb::Value::null()
    });

    REQUIRE(row.size() == 7);
    REQUIRE(row.get(0).typeId() == seeddb::LogicalTypeId::INTEGER);
    REQUIRE(row.get(1).typeId() == seeddb::LogicalTypeId::BIGINT);
    REQUIRE(row.get(2).typeId() == seeddb::LogicalTypeId::FLOAT);
    REQUIRE(row.get(3).typeId() == seeddb::LogicalTypeId::DOUBLE);
    REQUIRE(row.get(4).typeId() == seeddb::LogicalTypeId::VARCHAR);
    REQUIRE(row.get(5).typeId() == seeddb::LogicalTypeId::BOOLEAN);
    REQUIRE(row.get(6).typeId() == seeddb::LogicalTypeId::SQL_NULL);
}

// =============================================================================
// Schema Class Tests
// =============================================================================

TEST_CASE("ColumnSchema constructs with name and type", "[storage][schema]") {
    seeddb::ColumnSchema col("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER));
    REQUIRE(col.name() == "id");
    REQUIRE(col.type().id() == seeddb::LogicalTypeId::INTEGER);
}

TEST_CASE("ColumnSchema nullable flag can be set", "[storage][schema]") {
    SECTION("Default nullable is true") {
        seeddb::ColumnSchema col("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR));
        REQUIRE(col.isNullable());
    }

    SECTION("Nullable set to false") {
        seeddb::ColumnSchema col("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false);
        REQUIRE(!col.isNullable());
    }

    SECTION("Nullable set to true explicitly") {
        seeddb::ColumnSchema col("value", seeddb::LogicalType(seeddb::LogicalTypeId::DOUBLE), true);
        REQUIRE(col.isNullable());
    }
}

TEST_CASE("Schema default constructs empty", "[storage][schema]") {
    seeddb::Schema schema;
    REQUIRE(schema.columnCount() == 0);
}

TEST_CASE("Schema constructs from column vector", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR)));
    columns.push_back(seeddb::ColumnSchema("active", seeddb::LogicalType(seeddb::LogicalTypeId::BOOLEAN)));

    seeddb::Schema schema(columns);
    REQUIRE(schema.columnCount() == 3);
}

TEST_CASE("column() by index returns column", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR)));

    seeddb::Schema schema(columns);

    REQUIRE(schema.column(0).name() == "id");
    REQUIRE(schema.column(0).type().id() == seeddb::LogicalTypeId::INTEGER);
    REQUIRE(!schema.column(0).isNullable());

    REQUIRE(schema.column(1).name() == "name");
    REQUIRE(schema.column(1).type().id() == seeddb::LogicalTypeId::VARCHAR);
    REQUIRE(schema.column(1).isNullable());
}

TEST_CASE("column() by name returns column", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR)));

    seeddb::Schema schema(columns);

    REQUIRE(schema.column("id").name() == "id");
    REQUIRE(schema.column("id").type().id() == seeddb::LogicalTypeId::INTEGER);

    REQUIRE(schema.column("name").name() == "name");
    REQUIRE(schema.column("name").type().id() == seeddb::LogicalTypeId::VARCHAR);
}

TEST_CASE("hasColumn() finds columns", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER)));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR)));

    seeddb::Schema schema(columns);

    REQUIRE(schema.hasColumn("id"));
    REQUIRE(schema.hasColumn("name"));
    REQUIRE(!schema.hasColumn("nonexistent"));
}

TEST_CASE("columnIndex() returns index or nullopt", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER)));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR)));
    columns.push_back(seeddb::ColumnSchema("value", seeddb::LogicalType(seeddb::LogicalTypeId::DOUBLE)));

    seeddb::Schema schema(columns);

    REQUIRE(schema.columnIndex("id").has_value());
    REQUIRE(schema.columnIndex("id").value() == 0);

    REQUIRE(schema.columnIndex("name").has_value());
    REQUIRE(schema.columnIndex("name").value() == 1);

    REQUIRE(schema.columnIndex("value").has_value());
    REQUIRE(schema.columnIndex("value").value() == 2);

    REQUIRE(!schema.columnIndex("nonexistent").has_value());
}

TEST_CASE("validateRow() checks column count", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER)));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR)));

    seeddb::Schema schema(columns);

    SECTION("Correct column count") {
        seeddb::Row row({seeddb::Value::integer(1), seeddb::Value::varchar("test")});
        REQUIRE(schema.validateRow(row));
    }

    SECTION("Too few columns") {
        seeddb::Row row({seeddb::Value::integer(1)});
        REQUIRE(!schema.validateRow(row));
    }

    SECTION("Too many columns") {
        seeddb::Row row({seeddb::Value::integer(1), seeddb::Value::varchar("test"), seeddb::Value::integer(3)});
        REQUIRE(!schema.validateRow(row));
    }
}

TEST_CASE("validateRow() accepts NULL for nullable columns", "[storage][schema]") {
    std::vector<seeddb::ColumnSchema> columns;
    columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false));
    columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR), true));

    seeddb::Schema schema(columns);

    SECTION("NULL in nullable column is valid") {
        seeddb::Row row({seeddb::Value::integer(1), seeddb::Value::null()});
        REQUIRE(schema.validateRow(row));
    }

    SECTION("NULL in non-nullable column is invalid") {
        seeddb::Row row({seeddb::Value::null(), seeddb::Value::varchar("test")});
        REQUIRE(!schema.validateRow(row));
    }

    SECTION("Non-null values in all columns is valid") {
        seeddb::Row row({seeddb::Value::integer(1), seeddb::Value::varchar("test")});
        REQUIRE(schema.validateRow(row));
    }
}

TEST_CASE("toString() formats schema", "[storage][schema]") {
    SECTION("Empty schema") {
        seeddb::Schema schema;
        REQUIRE(schema.toString() == "");
    }

    SECTION("Single column") {
        std::vector<seeddb::ColumnSchema> columns;
        columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false));

        seeddb::Schema schema(columns);
        REQUIRE(schema.toString() == "id INTEGER NOT NULL");
    }

    SECTION("Multiple columns") {
        std::vector<seeddb::ColumnSchema> columns;
        columns.push_back(seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false));
        columns.push_back(seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR), true));
        columns.push_back(seeddb::ColumnSchema("active", seeddb::LogicalType(seeddb::LogicalTypeId::BOOLEAN), false));

        seeddb::Schema schema(columns);
        REQUIRE(schema.toString() == "id INTEGER NOT NULL, name VARCHAR, active BOOLEAN NOT NULL");
    }
}

TEST_CASE("logical_type_name helper function", "[storage][schema]") {
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::SQL_NULL) == std::string("SQL_NULL"));
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::INTEGER) == std::string("INTEGER"));
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::BIGINT) == std::string("BIGINT"));
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::FLOAT) == std::string("FLOAT"));
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::DOUBLE) == std::string("DOUBLE"));
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::VARCHAR) == std::string("VARCHAR"));
    REQUIRE(seeddb::logical_type_name(seeddb::LogicalTypeId::BOOLEAN) == std::string("BOOLEAN"));
}

// =============================================================================
// Table Class Tests
// =============================================================================

TEST_CASE("Table constructs with name and schema", "[storage][table]") {
    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false),
        seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR))
    });

    seeddb::Table table("users", schema);
    REQUIRE(table.name() == "users");
    REQUIRE(table.schema().columnCount() == 2);
}

TEST_CASE("Table - schema-only container", "[storage][table]") {
    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false),
        seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR), true),
    });
    seeddb::Table table("users", schema);

    REQUIRE(table.name() == "users");
    REQUIRE(table.schema().columnCount() == 2);
    REQUIRE(table.schema().column(0).name() == "id");
}

// =============================================================================
// Catalog Class Tests
// =============================================================================

TEST_CASE("Catalog default constructs empty", "[storage][catalog]") {
    seeddb::Catalog catalog;
    REQUIRE(catalog.tableCount() == 0);
}

TEST_CASE("Catalog createTable", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER), false),
        seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR))
    });

    SECTION("Create new table returns true") {
        bool result = catalog.createTable("users", seeddb::Schema(schema));
        REQUIRE(result);
        REQUIRE(catalog.tableCount() == 1);
        REQUIRE(catalog.hasTable("users"));
    }

    SECTION("Create duplicate table returns false") {
        catalog.createTable("users", seeddb::Schema(schema));
        bool result = catalog.createTable("users", seeddb::Schema(schema));
        REQUIRE(!result);
        REQUIRE(catalog.tableCount() == 1);
    }
}

TEST_CASE("Catalog dropTable", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER))
    });

    catalog.createTable("test", seeddb::Schema(schema));

    SECTION("Drop existing table returns true") {
        bool result = catalog.dropTable("test");
        REQUIRE(result);
        REQUIRE(catalog.tableCount() == 0);
        REQUIRE(!catalog.hasTable("test"));
    }

    SECTION("Drop non-existent table returns false") {
        bool result = catalog.dropTable("nonexistent");
        REQUIRE(!result);
        REQUIRE(catalog.tableCount() == 1);
    }
}

TEST_CASE("Catalog hasTable", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER))
    });

    REQUIRE(!catalog.hasTable("users"));

    catalog.createTable("users", seeddb::Schema(schema));
    REQUIRE(catalog.hasTable("users"));
    REQUIRE(!catalog.hasTable("products"));
}

TEST_CASE("Catalog getTable mutable", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER)),
        seeddb::ColumnSchema("name", seeddb::LogicalType(seeddb::LogicalTypeId::VARCHAR))
    });

    catalog.createTable("users", seeddb::Schema(schema));

    seeddb::Table* table = catalog.getTable("users");
    REQUIRE(table != nullptr);
    REQUIRE(table->name() == "users");
    REQUIRE(table->schema().columnCount() == 2);
}

TEST_CASE("Catalog getTable const", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER))
    });

    catalog.createTable("test", seeddb::Schema(schema));
    const seeddb::Catalog& const_catalog = catalog;

    const seeddb::Table* table = const_catalog.getTable("test");
    REQUIRE(table != nullptr);
    REQUIRE(table->name() == "test");
}

TEST_CASE("Catalog getTable nonexistent returns nullptr", "[storage][catalog]") {
    seeddb::Catalog catalog;

    REQUIRE(catalog.getTable("nonexistent") == nullptr);

    const seeddb::Catalog& const_catalog = catalog;
    REQUIRE(const_catalog.getTable("nonexistent") == nullptr);
}

TEST_CASE("Catalog tableCount", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER))
    });

    REQUIRE(catalog.tableCount() == 0);

    catalog.createTable("t1", seeddb::Schema(schema));
    REQUIRE(catalog.tableCount() == 1);

    catalog.createTable("t2", seeddb::Schema(schema));
    REQUIRE(catalog.tableCount() == 2);

    catalog.dropTable("t1");
    REQUIRE(catalog.tableCount() == 1);
}

TEST_CASE("Catalog iterator support", "[storage][catalog]") {
    seeddb::Catalog catalog;

    seeddb::Schema schema({
        seeddb::ColumnSchema("id", seeddb::LogicalType(seeddb::LogicalTypeId::INTEGER))
    });

    catalog.createTable("users", seeddb::Schema(schema));
    catalog.createTable("products", seeddb::Schema(schema));

    SECTION("Range-based for loop") {
        std::vector<std::string> names;
        for (const auto& [name, table] : catalog) {
            names.push_back(name);
            REQUIRE(table != nullptr);
        }
        REQUIRE(names.size() == 2);
        // Note: unordered_map iteration order is not guaranteed
    }

    SECTION("Iterator yields correct count") {
        size_t count = 0;
        for (const auto& pair : catalog) {
            (void)pair;  // suppress unused warning
            ++count;
        }
        REQUIRE(count == 2);
    }
}
