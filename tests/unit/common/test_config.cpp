#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <filesystem>
#include "common/config.h"

TEST_CASE("Config can be created with defaults", "[config]") {
    seeddb::Config config;
    REQUIRE(config.port() == 5432);
    REQUIRE(config.max_connections() == 100);
    REQUIRE(config.log_level() == "INFO");
}

TEST_CASE("Config can load from file", "[config]") {
    const std::string config_path = "/tmp/seeddb_test.conf";

    // Write test config
    std::ofstream file(config_path);
    file << "port = 5433\n";
    file << "max_connections = 50\n";
    file << "log_level = DEBUG\n";
    file << "data_directory = /var/lib/seeddb\n";
    file.close();

    seeddb::Config config;
    REQUIRE(config.load(config_path) == true);

    REQUIRE(config.port() == 5433);
    REQUIRE(config.max_connections() == 50);
    REQUIRE(config.log_level() == "DEBUG");
    REQUIRE(config.data_directory() == "/var/lib/seeddb");

    std::filesystem::remove(config_path);
}

TEST_CASE("Config ignores comments and blank lines", "[config]") {
    const std::string config_path = "/tmp/seeddb_test_comments.conf";

    std::ofstream file(config_path);
    file << "# This is a comment\n";
    file << "\n";
    file << "port = 5434  # inline comment\n";
    file << "  \n";
    file << "# Another comment\n";
    file.close();

    seeddb::Config config;
    REQUIRE(config.load(config_path) == true);
    REQUIRE(config.port() == 5434);

    std::filesystem::remove(config_path);
}

TEST_CASE("Config returns default for missing keys", "[config]") {
    seeddb::Config config;
    REQUIRE(config.get("nonexistent_key", "default_value") == "default_value");
}

TEST_CASE("Config handles invalid file gracefully", "[config]") {
    seeddb::Config config;
    REQUIRE(config.load("/nonexistent/path/config.conf") == false);
    // Should still have defaults
    REQUIRE(config.port() == 5432);
}

TEST_CASE("Config can set values programmatically", "[config]") {
    seeddb::Config config;
    config.set("custom_key", "custom_value");
    REQUIRE(config.get("custom_key", "") == "custom_value");
}

TEST_CASE("Config get_int handles valid integers", "[config]") {
    seeddb::Config config;
    config.set("test_int", "42");
    REQUIRE(config.get_int("test_int", 0) == 42);
}

TEST_CASE("Config get_int returns default for invalid values", "[config]") {
    seeddb::Config config;
    config.set("invalid_int", "not_a_number");
    REQUIRE(config.get_int("invalid_int", 999) == 999);
}

TEST_CASE("Config get_bool handles various formats", "[config]") {
    seeddb::Config config;

    config.set("bool_true1", "true");
    config.set("bool_true2", "TRUE");
    config.set("bool_true3", "1");
    config.set("bool_true4", "yes");
    config.set("bool_true5", "on");

    config.set("bool_false1", "false");
    config.set("bool_false2", "0");
    config.set("bool_false3", "no");
    config.set("bool_false4", "off");

    REQUIRE(config.get_bool("bool_true1", false) == true);
    REQUIRE(config.get_bool("bool_true2", false) == true);
    REQUIRE(config.get_bool("bool_true3", false) == true);
    REQUIRE(config.get_bool("bool_true4", false) == true);
    REQUIRE(config.get_bool("bool_true5", false) == true);

    REQUIRE(config.get_bool("bool_false1", true) == false);
    REQUIRE(config.get_bool("bool_false2", true) == false);
    REQUIRE(config.get_bool("bool_false3", true) == false);
    REQUIRE(config.get_bool("bool_false4", true) == false);
}
