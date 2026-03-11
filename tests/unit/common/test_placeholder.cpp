#include <catch2/catch_test_macros.hpp>

TEST_CASE("Placeholder test - Catch2 integration", "[setup]") {
    REQUIRE(true == true);
}

TEST_CASE("Basic arithmetic works", "[setup]") {
    REQUIRE(1 + 1 == 2);
}
