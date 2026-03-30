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

TEST_CASE("HeapTableIterator - single page with multiple rows", "[table_iterator]") {
    IterFixture fix;

    fix.writePageWithRows({
        makeIterRow(1, "Alice"),
        makeIterRow(2, "Bob"),
        makeIterRow(3, "Carol"),
    });

    auto iter = seeddb::HeapTableIterator(fix.file_id, 1, *fix.pool, fix.schema);

    REQUIRE(iter.next());
    REQUIRE(iter.currentRow().get(0).asInt32() == 1);
    REQUIRE(iter.currentRow().get(1).asString() == "Alice");
    REQUIRE(iter.currentTID().page_num == 0);
    REQUIRE(iter.currentTID().slot_id == 0);

    REQUIRE(iter.next());
    REQUIRE(iter.currentRow().get(0).asInt32() == 2);
    REQUIRE(iter.currentTID().slot_id == 1);

    REQUIRE(iter.next());
    REQUIRE(iter.currentRow().get(0).asInt32() == 3);
    REQUIRE(iter.currentTID().slot_id == 2);

    REQUIRE_FALSE(iter.next());
    // Subsequent calls still return false
    REQUIRE_FALSE(iter.next());
}

TEST_CASE("HeapTableIterator - multi-page iteration", "[table_iterator]") {
    IterFixture fix;

    // Fill page 0 to capacity
    std::vector<Row> page0_rows;
    PageId pid0 = fix.page_mgr->allocatePage(fix.file_id);
    Page page0(pid0, seeddb::PageType::DATA_PAGE);
    int row_count = 0;
    while (true) {
        Row row = makeIterRow(row_count, "row" + std::to_string(row_count));
        auto data = seeddb::RowSerializer::serialize(row, fix.schema);
        if (!page0.insertRecord(data.data(), static_cast<uint16_t>(data.size())).has_value()) {
            break;
        }
        row_count++;
    }
    fix.page_mgr->writePage(pid0, page0);

    // Add one row to page 1
    PageId pid1 = fix.page_mgr->allocatePage(fix.file_id);
    Page page1(pid1, seeddb::PageType::DATA_PAGE);
    Row extra = makeIterRow(row_count, "extra");
    auto extra_data = seeddb::RowSerializer::serialize(extra, fix.schema);
    page1.insertRecord(extra_data.data(), static_cast<uint16_t>(extra_data.size()));
    fix.page_mgr->writePage(pid1, page1);

    // Iterate all rows
    auto iter = seeddb::HeapTableIterator(fix.file_id, 2, *fix.pool, fix.schema);
    int seen = 0;
    while (iter.next()) {
        REQUIRE(iter.currentRow().get(0).asInt32() == seen);
        seen++;
    }
    REQUIRE(seen == row_count + 1);

    // Verify last row is on page 1
    auto iter2 = seeddb::HeapTableIterator(fix.file_id, 2, *fix.pool, fix.schema);
    seeddb::TID last_tid;
    while (iter2.next()) {
        last_tid = iter2.currentTID();
    }
    REQUIRE(last_tid.page_num == pid1.pageNum());
}

TEST_CASE("HeapTableIterator - skips deleted slots", "[table_iterator]") {
    IterFixture fix;

    // Create a page with 3 rows, delete slot 1
    PageId pid = fix.page_mgr->allocatePage(fix.file_id);
    Page page(pid, seeddb::PageType::DATA_PAGE);
    for (int i = 0; i < 3; ++i) {
        Row row = makeIterRow(i, "row" + std::to_string(i));
        auto data = seeddb::RowSerializer::serialize(row, fix.schema);
        page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));
    }
    page.deleteRecord(1);  // delete middle row
    fix.page_mgr->writePage(pid, page);

    auto iter = seeddb::HeapTableIterator(fix.file_id, 1, *fix.pool, fix.schema);

    REQUIRE(iter.next());
    REQUIRE(iter.currentRow().get(0).asInt32() == 0);
    REQUIRE(iter.currentTID().slot_id == 0);

    REQUIRE(iter.next());
    REQUIRE(iter.currentRow().get(0).asInt32() == 2);
    REQUIRE(iter.currentTID().slot_id == 2);  // slot 1 deleted, skipped

    REQUIRE_FALSE(iter.next());
}

TEST_CASE("HeapTableIterator - TID tracking across pages", "[table_iterator]") {
    IterFixture fix;

    // Two pages, one row each
    uint32_t pn0 = fix.writePageWithRows({makeIterRow(10, "first")});
    uint32_t pn1 = fix.writePageWithRows({makeIterRow(20, "second")});

    auto iter = seeddb::HeapTableIterator(fix.file_id, 2, *fix.pool, fix.schema);

    REQUIRE(iter.next());
    REQUIRE(iter.currentTID().file_id == fix.file_id);
    REQUIRE(iter.currentTID().page_num == pn0);
    REQUIRE(iter.currentTID().slot_id == 0);

    REQUIRE(iter.next());
    REQUIRE(iter.currentTID().page_num == pn1);
    REQUIRE(iter.currentTID().slot_id == 0);

    REQUIRE_FALSE(iter.next());
}

// =============================================================================
// Edge case tests — Section 7 robustness
// =============================================================================

TEST_CASE("HeapTableIterator - all slots deleted returns no rows",
          "[table_iterator]") {
    IterFixture fix;

    // Create a page with 3 rows, delete all
    PageId pid = fix.page_mgr->allocatePage(fix.file_id);
    Page page(pid, seeddb::PageType::DATA_PAGE);
    for (int i = 0; i < 3; ++i) {
        Row row = makeIterRow(i, "row" + std::to_string(i));
        auto data = seeddb::RowSerializer::serialize(row, fix.schema);
        page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));
    }
    page.deleteRecord(0);
    page.deleteRecord(1);
    page.deleteRecord(2);
    fix.page_mgr->writePage(pid, page);

    auto iter = seeddb::HeapTableIterator(fix.file_id, 1, *fix.pool, fix.schema);
    REQUIRE_FALSE(iter.next());
}

TEST_CASE("HeapTableIterator - single row insert-delete-insert cycle",
          "[table_iterator]") {
    IterFixture fix;

    // Page 0: one row
    PageId pid = fix.page_mgr->allocatePage(fix.file_id);
    Page page(pid, seeddb::PageType::DATA_PAGE);
    Row row1 = makeIterRow(1, "first");
    auto data1 = seeddb::RowSerializer::serialize(row1, fix.schema);
    auto slot1 = page.insertRecord(data1.data(), static_cast<uint16_t>(data1.size()));
    REQUIRE(slot1.has_value());

    // Delete it, then insert a new row (gets a new slot)
    page.deleteRecord(*slot1);
    Row row2 = makeIterRow(2, "second");
    auto data2 = seeddb::RowSerializer::serialize(row2, fix.schema);
    auto slot2 = page.insertRecord(data2.data(), static_cast<uint16_t>(data2.size()));
    REQUIRE(slot2.has_value());

    fix.page_mgr->writePage(pid, page);

    auto iter = seeddb::HeapTableIterator(fix.file_id, 1, *fix.pool, fix.schema);

    // Should find exactly one live row
    REQUIRE(iter.next());
    REQUIRE(iter.currentRow().get(0).asInt32() == 2);
    REQUIRE(iter.currentRow().get(1).asString() == "second");
    REQUIRE(iter.currentTID().slot_id == *slot2);

    REQUIRE_FALSE(iter.next());
}

TEST_CASE("HeapTableIterator - many pages with small buffer pool",
          "[table_iterator]") {
    // Use a dedicated fixture with a 3-frame buffer pool
    static int counter = 0;
    std::string dir = fs::temp_directory_path().string() + "/seeddb_iter_many_" +
                      std::to_string(++counter);
    fs::create_directories(dir);

    seeddb::Config config;
    config.set("buffer_pool_size", "3");

    auto page_mgr = std::make_unique<seeddb::PageManager>(dir);
    auto pool = std::make_unique<seeddb::BufferPool>(*page_mgr, config);
    Schema schema = makeIterSchema();

    uint32_t file_id = page_mgr->createTableFile("many_pages");

    // Create 10 pages, 5 rows each = 50 rows total
    const int num_pages = 10;
    const int rows_per_page = 5;
    for (int p = 0; p < num_pages; ++p) {
        PageId pid = page_mgr->allocatePage(file_id);
        Page page(pid, seeddb::PageType::DATA_PAGE);
        for (int r = 0; r < rows_per_page; ++r) {
            int id = p * rows_per_page + r;
            Row row = makeIterRow(id, "p" + std::to_string(p) + "_r" + std::to_string(r));
            auto data = seeddb::RowSerializer::serialize(row, schema);
            page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));
        }
        page_mgr->writePage(pid, page);
    }

    // Iterate with only 3 buffer frames — should evict and reload pages
    auto iter = seeddb::HeapTableIterator(file_id, num_pages, *pool, schema);
    int count = 0;
    int last_id = -1;
    while (iter.next()) {
        int id = iter.currentRow().get(0).asInt32();
        REQUIRE(id > last_id);  // Rows should be in insertion order
        last_id = id;
        count++;
    }
    REQUIRE(count == num_pages * rows_per_page);  // 50

    pool.reset();
    page_mgr.reset();
    fs::remove_all(dir);
}

TEST_CASE("HeapTableIterator - calling currentRow before next returns default",
          "[table_iterator]") {
    IterFixture fix;
    fix.writePageWithRows({makeIterRow(1, "Alice")});

    auto iter = seeddb::HeapTableIterator(fix.file_id, 1, *fix.pool, fix.schema);
    // currentRow() before any next() — should return default-constructed Row
    const Row& row = iter.currentRow();
    REQUIRE(row.size() == 0);
}

TEST_CASE("HeapTableIterator - currentTID before next returns invalid TID",
          "[table_iterator]") {
    IterFixture fix;
    fix.writePageWithRows({makeIterRow(1, "Alice")});

    auto iter = seeddb::HeapTableIterator(fix.file_id, 1, *fix.pool, fix.schema);
    TID tid = iter.currentTID();
    REQUIRE_FALSE(tid.isValid());
}
