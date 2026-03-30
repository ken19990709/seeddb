#include <catch2/catch_test_macros.hpp>

#include "storage/page_header.h"

#include <cstring>

// =============================================================================
// PageHeader Tests
// =============================================================================

TEST_CASE("PageHeader default constructs with zero values", "[storage][page_header]") {
    seeddb::PageHeader hdr;
    REQUIRE(!hdr.page_id.is_valid());
    REQUIRE(hdr.free_space_offset == 0);
    REQUIRE(hdr.slot_count == 0);
    REQUIRE(hdr.lsn == 0);
    REQUIRE(!hdr.prev_page.is_valid());
    REQUIRE(!hdr.next_page.is_valid());
}

TEST_CASE("PageHeader size is exactly HEADER_SIZE bytes", "[storage][page_header]") {
    // The serialized header must fit within HEADER_SIZE
    REQUIRE(seeddb::PageHeader::HEADER_SIZE >= sizeof(seeddb::PageHeader));
}

TEST_CASE("PageHeader serializes and deserializes correctly", "[storage][page_header]") {
    seeddb::PageHeader original;
    original.page_id           = seeddb::PageId(2, 5);
    original.free_space_offset = 3000;
    original.slot_count        = 7;
    original.page_type         = seeddb::PageType::DATA_PAGE;
    original.checksum          = 0xDEADBEEF;
    original.lsn               = 123456789ULL;
    original.prev_page         = seeddb::PageId(2, 4);
    original.next_page         = seeddb::PageId(2, 6);

    // Serialize into a buffer
    std::array<char, seeddb::PageHeader::HEADER_SIZE> buf{};
    original.serialize(buf.data());

    // Deserialize back
    seeddb::PageHeader restored;
    restored.deserialize(buf.data());

    REQUIRE(restored.page_id.fileId()  == 2);
    REQUIRE(restored.page_id.pageNum() == 5);
    REQUIRE(restored.free_space_offset == 3000);
    REQUIRE(restored.slot_count        == 7);
    REQUIRE(restored.page_type         == seeddb::PageType::DATA_PAGE);
    REQUIRE(restored.checksum          == 0xDEADBEEF);
    REQUIRE(restored.lsn               == 123456789ULL);
    REQUIRE(restored.prev_page.fileId()  == 2);
    REQUIRE(restored.prev_page.pageNum() == 4);
    REQUIRE(restored.next_page.fileId()  == 2);
    REQUIRE(restored.next_page.pageNum() == 6);
}

TEST_CASE("PageHeader serializes INDEX_PAGE type", "[storage][page_header]") {
    seeddb::PageHeader original;
    original.page_id   = seeddb::PageId(0, 0);
    original.page_type = seeddb::PageType::INDEX_PAGE;

    std::array<char, seeddb::PageHeader::HEADER_SIZE> buf{};
    original.serialize(buf.data());

    seeddb::PageHeader restored;
    restored.deserialize(buf.data());

    REQUIRE(restored.page_type == seeddb::PageType::INDEX_PAGE);
}

TEST_CASE("PageHeader serializes OVERFLOW_PAGE type", "[storage][page_header]") {
    seeddb::PageHeader original;
    original.page_id   = seeddb::PageId(0, 1);
    original.page_type = seeddb::PageType::OVERFLOW_PAGE;

    std::array<char, seeddb::PageHeader::HEADER_SIZE> buf{};
    original.serialize(buf.data());

    seeddb::PageHeader restored;
    restored.deserialize(buf.data());

    REQUIRE(restored.page_type == seeddb::PageType::OVERFLOW_PAGE);
}

TEST_CASE("PageHeader prev_page and next_page invalid by default", "[storage][page_header]") {
    seeddb::PageHeader hdr;
    hdr.page_id = seeddb::PageId(1, 0);  // first page, no prev/next

    std::array<char, seeddb::PageHeader::HEADER_SIZE> buf{};
    hdr.serialize(buf.data());

    seeddb::PageHeader restored;
    restored.deserialize(buf.data());

    REQUIRE(!restored.prev_page.is_valid());
    REQUIRE(!restored.next_page.is_valid());
}
