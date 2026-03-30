#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>
#include "storage/tid.h"

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
