#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <filesystem>
#include "common/logger.h"

TEST_CASE("Logger has correct log levels", "[logger]") {
    REQUIRE(static_cast<int>(seeddb::LogLevel::DEBUG) < static_cast<int>(seeddb::LogLevel::INFO));
    REQUIRE(static_cast<int>(seeddb::LogLevel::INFO) < static_cast<int>(seeddb::LogLevel::WARN));
    REQUIRE(static_cast<int>(seeddb::LogLevel::WARN) < static_cast<int>(seeddb::LogLevel::ERROR));
    REQUIRE(static_cast<int>(seeddb::LogLevel::ERROR) < static_cast<int>(seeddb::LogLevel::FATAL));
}

TEST_CASE("Logger singleton can be accessed", "[logger]") {
    seeddb::Logger& logger = seeddb::Logger::instance();
    REQUIRE(&logger == &seeddb::Logger::instance());
}

TEST_CASE("Logger can set log level", "[logger]") {
    seeddb::Logger& logger = seeddb::Logger::instance();
    logger.set_level(seeddb::LogLevel::DEBUG);
    REQUIRE(logger.level() == seeddb::LogLevel::DEBUG);

    logger.set_level(seeddb::LogLevel::WARN);
    REQUIRE(logger.level() == seeddb::LogLevel::WARN);
}

TEST_CASE("Logger writes to file", "[logger]") {
    const std::string test_log = "/tmp/seeddb_test.log";

    // Clean up if exists
    std::filesystem::remove(test_log);

    seeddb::Logger& logger = seeddb::Logger::instance();
    logger.set_level(seeddb::LogLevel::DEBUG);
    REQUIRE(logger.open_file(test_log) == true);

    logger.info("Test message");
    logger.flush();

    // Verify file exists and contains message
    std::ifstream file(test_log);
    REQUIRE(file.is_open());

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    REQUIRE(content.find("Test message") != std::string::npos);
    REQUIRE(content.find("INFO") != std::string::npos);

    file.close();
    std::filesystem::remove(test_log);
}

TEST_CASE("Logger respects log level filtering", "[logger]") {
    const std::string test_log = "/tmp/seeddb_test_level.log";
    std::filesystem::remove(test_log);

    seeddb::Logger& logger = seeddb::Logger::instance();
    logger.set_level(seeddb::LogLevel::WARN);
    REQUIRE(logger.open_file(test_log) == true);

    logger.debug("This should not appear");
    logger.info("This should not appear");
    logger.warn("This should appear");
    logger.error("This should also appear");
    logger.flush();

    std::ifstream file(test_log);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    REQUIRE(content.find("should not appear") == std::string::npos);
    REQUIRE(content.find("should appear") != std::string::npos);
    REQUIRE(content.find("should also appear") != std::string::npos);

    file.close();
    std::filesystem::remove(test_log);
}

TEST_CASE("Log macros work", "[logger]") {
    const std::string test_log = "/tmp/seeddb_test_macros.log";
    std::filesystem::remove(test_log);

    seeddb::Logger& logger = seeddb::Logger::instance();
    logger.set_level(seeddb::LogLevel::DEBUG);
    logger.open_file(test_log);

    SEEDDB_DEBUG("Debug via macro");
    SEEDDB_INFO("Info via macro");
    SEEDDB_WARN("Warn via macro");
    SEEDDB_ERROR("Error via macro");
    logger.flush();

    std::ifstream file(test_log);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    REQUIRE(content.find("Debug via macro") != std::string::npos);
    REQUIRE(content.find("Info via macro") != std::string::npos);
    REQUIRE(content.find("Warn via macro") != std::string::npos);
    REQUIRE(content.find("Error via macro") != std::string::npos);

    file.close();
    std::filesystem::remove(test_log);
}
