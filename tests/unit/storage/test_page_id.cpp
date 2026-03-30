#include <catch2/catch_test_macros.hpp>

#include "storage/page_id.h"

#include <unordered_set>

// =============================================================================
// PageId Tests
// =============================================================================

namespace {
// Helper to avoid Catch2's expression decomposition issues
bool pageIdsEqual(const seeddb::PageId& a, const seeddb::PageId& b) {
    return a == b;
}
}

TEST_CASE("PageId default constructs to invalid", "[storage][page_id]") {
    seeddb::PageId page_id;
    REQUIRE(!page_id.is_valid());
}

TEST_CASE("PageId constructs with file_id and page_num", "[storage][page_id]") {
    seeddb::PageId page_id(1, 42);
    REQUIRE(page_id.fileId() == 1);
    REQUIRE(page_id.pageNum() == 42);
    REQUIRE(page_id.is_valid());
}

TEST_CASE("PageId equality comparison", "[storage][page_id]") {
    seeddb::PageId a(1, 10);
    seeddb::PageId b(1, 10);
    seeddb::PageId c(1, 20);
    seeddb::PageId d(2, 10);

    REQUIRE(a == b);
    REQUIRE(!(a != b));

    REQUIRE(a != c);  // same file, different page
    REQUIRE(a != d);  // different file, same page
}

TEST_CASE("PageId less-than comparison for ordering", "[storage][page_id]") {
    seeddb::PageId a(1, 10);
    seeddb::PageId b(1, 20);
    seeddb::PageId c(2, 5);

    // Same file, different page
    REQUIRE(a < b);
    REQUIRE(!(b < a));

    // Different file (file_id takes priority)
    REQUIRE(a < c);
    REQUIRE(b < c);
}

TEST_CASE("PageId can be used as unordered_map key", "[storage][page_id]") {
    seeddb::PageId p1(1, 10);
    seeddb::PageId p2(1, 20);
    seeddb::PageId p3(2, 10);
    seeddb::PageId p1_dup(1, 10);

    // Verify equality works correctly
    REQUIRE(pageIdsEqual(p1, p1_dup));
    REQUIRE(!pageIdsEqual(p1, p2));

    // Verify hash works correctly (same PageId should have same hash)
    std::hash<seeddb::PageId> hasher;
    REQUIRE(hasher(p1) == hasher(p1_dup));

    std::unordered_set<seeddb::PageId> page_set;

    page_set.insert(p1);
    page_set.insert(p2);
    page_set.insert(p3);
    page_set.insert(p1_dup);  // duplicate, should not increase size

    REQUIRE(page_set.size() == 3);
    REQUIRE(page_set.count(p1) == 1);
    REQUIRE(page_set.count(p2) == 1);
    REQUIRE(page_set.count(p3) == 1);
}

TEST_CASE("PageId toString", "[storage][page_id]") {
    SECTION("Valid page ID") {
        seeddb::PageId page_id(3, 42);
        REQUIRE(page_id.toString() == "(3, 42)");
    }

    SECTION("Invalid page ID") {
        seeddb::PageId page_id;
        REQUIRE(page_id.toString() == "(INVALID)");
    }
}

TEST_CASE("PageId INVALID_PAGE_ID constant", "[storage][page_id]") {
    REQUIRE(!seeddb::INVALID_PAGE_ID.is_valid());
}

TEST_CASE("PageId offset calculation", "[storage][page_id]") {
    // Assuming PAGE_SIZE is 4096 (4KB)
    seeddb::PageId page_id(1, 5);
    // offset = page_num * PAGE_SIZE = 5 * 4096 = 20480
    REQUIRE(page_id.offset() == 5 * seeddb::PAGE_SIZE);
}

// =============================================================================
// PageType Tests
// =============================================================================

TEST_CASE("PageType enum values", "[storage][page_type]") {
    // Verify PageType enum values exist and are distinct
    REQUIRE(seeddb::PageType::DATA_PAGE != seeddb::PageType::INDEX_PAGE);
    REQUIRE(seeddb::PageType::INDEX_PAGE != seeddb::PageType::OVERFLOW_PAGE);
    REQUIRE(seeddb::PageType::DATA_PAGE != seeddb::PageType::OVERFLOW_PAGE);
}

TEST_CASE("PAGE_SIZE constant", "[storage][page_constants]") {
    // PAGE_SIZE should be 4096 (4KB)
    REQUIRE(seeddb::PAGE_SIZE == 4096);
}
