#include <catch2/catch_test_macros.hpp>

#include "storage/row.h"

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
