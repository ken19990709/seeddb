// =============================================================================
// Unit / integration tests for PageManager
// =============================================================================

#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>

#include "storage/page_manager.h"
#include "storage/page.h"

#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace seeddb;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

/// RAII helper that creates a unique temp directory and removes it on exit.
struct TempDir {
    std::string path;
    static int& counter() { static int c = 0; return c; }

    TempDir() {
        path = "/tmp/seeddb_pm_test_" + std::to_string(++counter());
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

// ---------------------------------------------------------------------------
// Tests: table file management
// ---------------------------------------------------------------------------

TEST_CASE("PageManager - create table file", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);

    uint32_t fid = pm.createTableFile("users");
    REQUIRE(fid != INVALID_FILE_ID);
    REQUIRE(pm.tableExists("users"));
}

TEST_CASE("PageManager - create table file twice throws", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);

    pm.createTableFile("users");
    REQUIRE_THROWS_AS(pm.createTableFile("users"), std::runtime_error);
}

TEST_CASE("PageManager - tableExists returns false for unknown table", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    REQUIRE_FALSE(pm.tableExists("no_such_table"));
}

TEST_CASE("PageManager - drop table file", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);

    pm.createTableFile("orders");
    REQUIRE(pm.tableExists("orders"));

    REQUIRE(pm.dropTableFile("orders"));
    REQUIRE_FALSE(pm.tableExists("orders"));
}

TEST_CASE("PageManager - drop non-existent table returns false", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    REQUIRE_FALSE(pm.dropTableFile("ghost"));
}

// ---------------------------------------------------------------------------
// Tests: page allocation
// ---------------------------------------------------------------------------

TEST_CASE("PageManager - allocate first page", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    uint32_t fid = pm.createTableFile("t1");

    PageId pid = pm.allocatePage(fid);
    REQUIRE(pid.is_valid());
    REQUIRE(pid.fileId() == fid);
    REQUIRE(pid.pageNum() == 0);
}

TEST_CASE("PageManager - allocate multiple pages", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    uint32_t fid = pm.createTableFile("t2");

    PageId p0 = pm.allocatePage(fid);
    PageId p1 = pm.allocatePage(fid);
    PageId p2 = pm.allocatePage(fid);

    REQUIRE(p0.pageNum() == 0);
    REQUIRE(p1.pageNum() == 1);
    REQUIRE(p2.pageNum() == 2);
}

// ---------------------------------------------------------------------------
// Tests: page read / write
// ---------------------------------------------------------------------------

TEST_CASE("PageManager - write and read page round-trip", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    uint32_t fid = pm.createTableFile("rw_table");
    PageId pid = pm.allocatePage(fid);

    // Build a page, insert one record
    Page page_out(pid, PageType::DATA_PAGE);
    const char* record = "hello page";
    auto slot = page_out.insertRecord(record, static_cast<uint16_t>(std::strlen(record)));
    REQUIRE(slot.has_value());

    REQUIRE(pm.writePage(pid, page_out));

    // Read back
    Page page_in;
    REQUIRE(pm.getPage(pid, page_in));

    auto [data, size] = page_in.getRecord(*slot);
    REQUIRE(size == std::strlen(record));
    REQUIRE(std::memcmp(data, record, size) == 0);
}

TEST_CASE("PageManager - write multiple records across pages", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    uint32_t fid = pm.createTableFile("multi_page");

    // Two pages, one record each
    PageId pid0 = pm.allocatePage(fid);
    PageId pid1 = pm.allocatePage(fid);

    Page p0(pid0, PageType::DATA_PAGE);
    Page p1(pid1, PageType::DATA_PAGE);

    p0.insertRecord("alpha", 5);
    p1.insertRecord("beta",  4);

    REQUIRE(pm.writePage(pid0, p0));
    REQUIRE(pm.writePage(pid1, p1));

    Page r0, r1;
    REQUIRE(pm.getPage(pid0, r0));
    REQUIRE(pm.getPage(pid1, r1));

    auto [d0, s0] = r0.getRecord(0);
    auto [d1, s1] = r1.getRecord(0);

    REQUIRE(s0 == 5);
    REQUIRE(std::memcmp(d0, "alpha", 5) == 0);
    REQUIRE(s1 == 4);
    REQUIRE(std::memcmp(d1, "beta", 4) == 0);
}

// ---------------------------------------------------------------------------
// Tests: persistence across PageManager instances
// ---------------------------------------------------------------------------

TEST_CASE("PageManager - data persists across close and reopen", "[page_manager]") {
    TempDir td;
    PageId pid;

    // --- Session 1: create table, write data ---
    {
        PageManager pm(td.path);
        uint32_t fid = pm.createTableFile("persist_tbl");
        pid = pm.allocatePage(fid);

        Page page_out(pid, PageType::DATA_PAGE);
        page_out.insertRecord("persistent!", 11);
        REQUIRE(pm.writePage(pid, page_out));
        // pm destructor closes all file handles
    }

    // --- Session 2: reopen table, read data ---
    {
        PageManager pm(td.path);
        uint32_t fid = pm.openTableFile("persist_tbl");
        REQUIRE(fid != INVALID_FILE_ID);

        // Re-construct the PageId with the new session's file_id
        PageId new_pid(fid, pid.pageNum());
        Page page_in;
        REQUIRE(pm.getPage(new_pid, page_in));

        auto [data, size] = page_in.getRecord(0);
        REQUIRE(size == 11);
        REQUIRE(std::memcmp(data, "persistent!", 11) == 0);
    }
}

TEST_CASE("PageManager - openTableFile returns INVALID for missing table", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);
    REQUIRE(pm.openTableFile("missing") == INVALID_FILE_ID);
}

TEST_CASE("PageManager - multiple tables coexist independently", "[page_manager]") {
    TempDir td;
    PageManager pm(td.path);

    uint32_t fid_a = pm.createTableFile("tableA");
    uint32_t fid_b = pm.createTableFile("tableB");
    REQUIRE(fid_a != fid_b);

    PageId pA = pm.allocatePage(fid_a);
    PageId pB = pm.allocatePage(fid_b);

    Page pageA(pA, PageType::DATA_PAGE);
    Page pageB(pB, PageType::DATA_PAGE);
    pageA.insertRecord("aaa", 3);
    pageB.insertRecord("bbb", 3);
    REQUIRE(pm.writePage(pA, pageA));
    REQUIRE(pm.writePage(pB, pageB));

    Page rA, rB;
    REQUIRE(pm.getPage(pA, rA));
    REQUIRE(pm.getPage(pB, rB));

    auto [dA, sA] = rA.getRecord(0);
    auto [dB, sB] = rB.getRecord(0);
    REQUIRE(std::memcmp(dA, "aaa", 3) == 0);
    REQUIRE(std::memcmp(dB, "bbb", 3) == 0);
}
