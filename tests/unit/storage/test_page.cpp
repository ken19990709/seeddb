#include <catch2/catch_test_macros.hpp>

#include "storage/page.h"

#include <cstring>
#include <string>
#include <string_view>

// =============================================================================
// Helpers
// =============================================================================

namespace {

/// Construct a Page with a given PageId and DATA_PAGE type.
seeddb::Page makeDataPage(uint32_t file_id, uint32_t page_num) {
    return seeddb::Page(seeddb::PageId(file_id, page_num), seeddb::PageType::DATA_PAGE);
}

/// Simple way to compare record bytes
bool recordEquals(const char* data, uint16_t size, std::string_view expected) {
    if (size != static_cast<uint16_t>(expected.size())) return false;
    return std::memcmp(data, expected.data(), size) == 0;
}

} // namespace

// =============================================================================
// Construction / initial state
// =============================================================================

TEST_CASE("Page initializes with correct header values", "[storage][page]") {
    auto page = makeDataPage(1, 0);
    const seeddb::PageHeader& hdr = page.header();

    REQUIRE(hdr.page_id.fileId()  == 1);
    REQUIRE(hdr.page_id.pageNum() == 0);
    REQUIRE(hdr.page_type         == seeddb::PageType::DATA_PAGE);
    REQUIRE(hdr.slot_count        == 0);
    // free_space_offset should point right after the header
    REQUIRE(hdr.free_space_offset == seeddb::PageHeader::HEADER_SIZE);
}

TEST_CASE("New page reports correct free space", "[storage][page]") {
    auto page = makeDataPage(1, 0);
    // Full page minus the header
    uint16_t expected_free = static_cast<uint16_t>(
        seeddb::PAGE_SIZE - seeddb::PageHeader::HEADER_SIZE);
    REQUIRE(page.freeSpace() == expected_free);
}

TEST_CASE("New page has no slots", "[storage][page]") {
    auto page = makeDataPage(1, 0);
    REQUIRE(page.slotCount() == 0);
}

// =============================================================================
// insertRecord
// =============================================================================

TEST_CASE("insertRecord stores data and returns a valid slot id", "[storage][page]") {
    auto page = makeDataPage(1, 1);
    std::string data = "hello";
    auto slot = page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));

    REQUIRE(slot.has_value());
    REQUIRE(*slot < page.slotCount());
}

TEST_CASE("insertRecord increases slot count", "[storage][page]") {
    auto page = makeDataPage(1, 2);
    REQUIRE(page.slotCount() == 0);

    page.insertRecord("a", 1);
    REQUIRE(page.slotCount() == 1);

    page.insertRecord("bb", 2);
    REQUIRE(page.slotCount() == 2);
}

TEST_CASE("insertRecord reduces free space", "[storage][page]") {
    auto page = makeDataPage(1, 3);
    uint16_t before = page.freeSpace();

    page.insertRecord("hello", 5);
    REQUIRE(page.freeSpace() < before);
}

TEST_CASE("insertRecord returns nullopt when page is full", "[storage][page]") {
    auto page = makeDataPage(1, 4);

    // Fill the page with records
    std::string big(200, 'x');
    int inserted = 0;
    while (true) {
        auto slot = page.insertRecord(big.data(), static_cast<uint16_t>(big.size()));
        if (!slot.has_value()) break;
        ++inserted;
        if (inserted > 1000) FAIL("insertRecord never signalled full page");
    }
    REQUIRE(inserted > 0);
}

// =============================================================================
// getRecord
// =============================================================================

TEST_CASE("getRecord returns the same data that was inserted", "[storage][page]") {
    auto page = makeDataPage(1, 5);
    std::string data = "world";
    auto slot = page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));
    REQUIRE(slot.has_value());

    auto [ptr, size] = page.getRecord(*slot);
    REQUIRE(ptr != nullptr);
    REQUIRE(recordEquals(ptr, size, data));
}

TEST_CASE("getRecord works for multiple records", "[storage][page]") {
    auto page = makeDataPage(1, 6);
    std::vector<std::string> records = {"alpha", "beta", "gamma", "delta"};
    std::vector<uint16_t> slots;

    for (const auto& r : records) {
        auto s = page.insertRecord(r.data(), static_cast<uint16_t>(r.size()));
        REQUIRE(s.has_value());
        slots.push_back(*s);
    }

    for (size_t i = 0; i < records.size(); ++i) {
        auto [ptr, size] = page.getRecord(slots[i]);
        REQUIRE(ptr != nullptr);
        REQUIRE(recordEquals(ptr, size, records[i]));
    }
}

TEST_CASE("getRecord returns nullptr for out-of-range slot", "[storage][page]") {
    auto page = makeDataPage(1, 7);
    auto [ptr, size] = page.getRecord(99);
    REQUIRE(ptr == nullptr);
    REQUIRE(size == 0);
}

// =============================================================================
// deleteRecord
// =============================================================================

TEST_CASE("deleteRecord makes slot invisible", "[storage][page]") {
    auto page = makeDataPage(1, 8);
    std::string data = "to delete";
    auto slot = page.insertRecord(data.data(), static_cast<uint16_t>(data.size()));
    REQUIRE(slot.has_value());

    bool ok = page.deleteRecord(*slot);
    REQUIRE(ok);

    auto [ptr, size] = page.getRecord(*slot);
    REQUIRE(ptr == nullptr);
}

TEST_CASE("deleteRecord returns false for invalid slot", "[storage][page]") {
    auto page = makeDataPage(1, 9);
    REQUIRE(!page.deleteRecord(99));
}

TEST_CASE("deleteRecord frees space after compact", "[storage][page]") {
    auto page = makeDataPage(1, 10);

    auto s1 = page.insertRecord("aaa", 3);
    auto s2 = page.insertRecord("bbb", 3);
    REQUIRE(s1.has_value());
    REQUIRE(s2.has_value());

    uint16_t before_delete = page.freeSpace();

    page.deleteRecord(*s1);
    page.compact();

    REQUIRE(page.freeSpace() > before_delete);
}

// =============================================================================
// compact
// =============================================================================

TEST_CASE("compact preserves remaining records", "[storage][page]") {
    auto page = makeDataPage(1, 11);

    auto s1 = page.insertRecord("keep1", 5);
    auto s2 = page.insertRecord("remove", 6);
    auto s3 = page.insertRecord("keep2", 5);
    REQUIRE(s1.has_value());
    REQUIRE(s2.has_value());
    REQUIRE(s3.has_value());

    page.deleteRecord(*s2);
    page.compact();

    // s1 and s3 should still be readable
    auto [p1, sz1] = page.getRecord(*s1);
    REQUIRE(p1 != nullptr);
    REQUIRE(recordEquals(p1, sz1, "keep1"));

    auto [p3, sz3] = page.getRecord(*s3);
    REQUIRE(p3 != nullptr);
    REQUIRE(recordEquals(p3, sz3, "keep2"));

    // s2 should be gone
    auto [p2, sz2] = page.getRecord(*s2);
    REQUIRE(p2 == nullptr);
}

// =============================================================================
// Serialization (raw buffer round-trip)
// =============================================================================

TEST_CASE("Page serializes and deserializes correctly", "[storage][page]") {
    auto page = makeDataPage(3, 7);
    page.insertRecord("record_one", 10);
    page.insertRecord("rec2", 4);

    // Serialize
    std::array<char, seeddb::PAGE_SIZE> buf{};
    page.serialize(buf.data());

    // Deserialize into a new page
    seeddb::Page restored;
    restored.deserialize(buf.data());

    REQUIRE(restored.header().page_id.fileId()  == 3);
    REQUIRE(restored.header().page_id.pageNum() == 7);
    REQUIRE(restored.slotCount() == 2);

    auto [p0, s0] = restored.getRecord(0);
    REQUIRE(p0 != nullptr);
    REQUIRE(recordEquals(p0, s0, "record_one"));

    auto [p1, s1] = restored.getRecord(1);
    REQUIRE(p1 != nullptr);
    REQUIRE(recordEquals(p1, s1, "rec2"));
}
