#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>
#include "storage/tid.h"
#include <filesystem>
#include <memory>
#include "storage/buffer/buffer_pool.h"
#include "storage/page.h"
#include "storage/page_manager.h"
#include "storage/row.h"
#include "storage/row_serializer.h"
#include "storage/schema.h"
#include "storage/table_iterator.h"
#include "common/config.h"
#include "common/value.h"

using namespace seeddb;

// =============================================================================
// TID tests
// =============================================================================

TEST_CASE("TID - default construction is invalid", "[tid]") {
    TID tid;
    REQUIRE_FALSE(tid.isValid());
    REQUIRE(tid.file_id == INVALID_FILE_ID);
    REQUIRE(tid.page_num == INVALID_PAGE_NUM);
    REQUIRE(tid.slot_id == 0);
}

TEST_CASE("TID - construction with valid values", "[tid]") {
    TID tid{1, 5, 3};
    REQUIRE(tid.isValid());
    REQUIRE(tid.file_id == 1);
    REQUIRE(tid.page_num == 5);
    REQUIRE(tid.slot_id == 3);
}

TEST_CASE("TID - validity depends on file_id", "[tid]") {
    TID valid{42, 0, 0};
    REQUIRE(valid.isValid());

    TID invalid{INVALID_FILE_ID, 100, 5};
    REQUIRE_FALSE(invalid.isValid());
}

namespace fs = std::filesystem;

// =============================================================================
// Test helpers
// =============================================================================

static Schema makeIterSchema() {
    return Schema({
        ColumnSchema("id",   LogicalType(LogicalTypeId::INTEGER), false),
        ColumnSchema("name", LogicalType(LogicalTypeId::VARCHAR), true),
    });
}

[[maybe_unused]] static Row makeIterRow(int id, const std::string& name) {
    return Row({Value::integer(id), Value::varchar(name)});
}

/// Fixture: creates temp dir, PageManager, BufferPool, test table file.
struct IterFixture {
    std::string dir;
    std::unique_ptr<seeddb::PageManager> page_mgr;
    std::unique_ptr<seeddb::BufferPool> pool;
    Schema schema;
    uint32_t file_id{seeddb::INVALID_FILE_ID};

    IterFixture()
        : schema(makeIterSchema())
    {
        dir = fs::temp_directory_path().string() + "/seeddb_iter_" +
              std::to_string(reinterpret_cast<uintptr_t>(this));
        fs::create_directories(dir);
        page_mgr = std::make_unique<seeddb::PageManager>(dir);
        pool = std::make_unique<seeddb::BufferPool>(*page_mgr, seeddb::Config{});
        file_id = page_mgr->createTableFile("iter_test");
    }

    ~IterFixture() {
        pool.reset();
        page_mgr.reset();
        fs::remove_all(dir);
    }

    /// Allocate a page, populate with rows, write to disk.
    /// Returns the page number of the allocated page.
    uint32_t writePageWithRows(const std::vector<Row>& rows) {
        PageId pid = page_mgr->allocatePage(file_id);
        Page page(pid, seeddb::PageType::DATA_PAGE);
        for (const auto& row : rows) {
            auto data = seeddb::RowSerializer::serialize(row, schema);
            page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));
        }
        page_mgr->writePage(pid, page);
        return pid.pageNum();
    }
};

TEST_CASE("HeapTableIterator - empty table returns no rows", "[table_iterator]") {
    IterFixture fix;
    // 0 pages allocated
    auto iter = seeddb::HeapTableIterator(fix.file_id, 0, *fix.pool, fix.schema);
    REQUIRE_FALSE(iter.next());
}
