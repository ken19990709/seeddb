#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include "common/types.h"

TEST_CASE("ObjectId is 4 bytes", "[types]") {
    REQUIRE(sizeof(seeddb::ObjectId) == 4);
}

TEST_CASE("TransactionId is 8 bytes", "[types]") {
    REQUIRE(sizeof(seeddb::TransactionId) == 8);
}

TEST_CASE("Lsn is 8 bytes", "[types]") {
    REQUIRE(sizeof(seeddb::Lsn) == 8);
}

TEST_CASE("PageId struct has correct size", "[types]") {
    REQUIRE(sizeof(seeddb::PageId) == 12);  // 3 * 4 bytes
}

TEST_CASE("SlotId is 2 bytes", "[types]") {
    REQUIRE(sizeof(seeddb::SlotId) == 2);
}

TEST_CASE("Invalid constants are defined", "[types]") {
    REQUIRE(seeddb::INVALID_OBJECT_ID == std::numeric_limits<seeddb::ObjectId>::max());
    REQUIRE(seeddb::INVALID_TRANSACTION_ID == std::numeric_limits<seeddb::TransactionId>::max());
    REQUIRE(seeddb::INVALID_LSN == std::numeric_limits<seeddb::Lsn>::max());
}

TEST_CASE("PageId equality operators work", "[types]") {
    seeddb::PageId p1 = {1, 2, 3};
    seeddb::PageId p2 = {1, 2, 3};
    seeddb::PageId p3 = {1, 2, 4};

    REQUIRE(p1 == p2);
    REQUIRE(p1 != p3);
}

TEST_CASE("Page constants are defined", "[types]") {
    REQUIRE(seeddb::DEFAULT_PAGE_SIZE == 8192);
    REQUIRE(seeddb::MAX_COLUMNS == 1600);
    REQUIRE(seeddb::MAX_IDENTIFIER_LENGTH == 63);
}
