// =============================================================================
// Unit tests for DiskManager
// =============================================================================

#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>

#include "storage/disk_manager.h"

#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace seeddb;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

/// Returns a unique path under /tmp for a test artefact.
static std::string tmpPath(const std::string& tag) {
    return "/tmp/seeddb_dm_" + tag + ".db";
}

/// RAII guard that removes a file on construction and destruction.
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& p) : path(p) {
        fs::remove(path);  // start clean
    }
    ~TempFile() { fs::remove(path); }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("DiskManager - open creates new file", "[disk_manager]") {
    TempFile tf(tmpPath("open_new"));
    DiskManager dm;

    REQUIRE_FALSE(dm.isOpen(0));
    REQUIRE(dm.openFile(0, tf.path));
    REQUIRE(dm.isOpen(0));
    REQUIRE(dm.pageCount(0) == 0);
    // Physical file should now exist
    REQUIRE(fs::exists(tf.path));
}

TEST_CASE("DiskManager - close file", "[disk_manager]") {
    TempFile tf(tmpPath("close"));
    DiskManager dm;
    REQUIRE(dm.openFile(0, tf.path));

    dm.closeFile(0);
    REQUIRE_FALSE(dm.isOpen(0));
}

TEST_CASE("DiskManager - allocate pages sequentially", "[disk_manager]") {
    TempFile tf(tmpPath("alloc"));
    DiskManager dm;
    REQUIRE(dm.openFile(1, tf.path));

    PageId p0 = dm.allocatePage(1);
    REQUIRE(p0.is_valid());
    REQUIRE(p0.fileId() == 1);
    REQUIRE(p0.pageNum() == 0);
    REQUIRE(dm.pageCount(1) == 1);

    PageId p1 = dm.allocatePage(1);
    REQUIRE(p1.pageNum() == 1);
    REQUIRE(dm.pageCount(1) == 2);

    PageId p2 = dm.allocatePage(1);
    REQUIRE(p2.pageNum() == 2);
    REQUIRE(dm.pageCount(1) == 3);
}

TEST_CASE("DiskManager - write and read round-trip", "[disk_manager]") {
    TempFile tf(tmpPath("rw"));
    DiskManager dm;
    REQUIRE(dm.openFile(2, tf.path));

    PageId pid = dm.allocatePage(2);
    REQUIRE(pid.is_valid());

    // Prepare write buffer with recognisable pattern
    char write_buf[PAGE_SIZE];
    std::memset(write_buf, 0, PAGE_SIZE);
    const char* msg = "Hello, DiskManager!";
    std::memcpy(write_buf, msg, std::strlen(msg) + 1);
    REQUIRE(dm.writePage(pid, write_buf));

    char read_buf[PAGE_SIZE];
    std::memset(read_buf, 0xFF, PAGE_SIZE);
    REQUIRE(dm.readPage(pid, read_buf));

    REQUIRE(std::memcmp(write_buf, read_buf, PAGE_SIZE) == 0);
}

TEST_CASE("DiskManager - newly allocated page contains zeros", "[disk_manager]") {
    TempFile tf(tmpPath("zero"));
    DiskManager dm;
    REQUIRE(dm.openFile(3, tf.path));

    PageId pid = dm.allocatePage(3);

    char buf[PAGE_SIZE];
    std::memset(buf, 0xFF, PAGE_SIZE);  // fill with non-zero
    REQUIRE(dm.readPage(pid, buf));

    char zeros[PAGE_SIZE];
    std::memset(zeros, 0, PAGE_SIZE);
    REQUIRE(std::memcmp(buf, zeros, PAGE_SIZE) == 0);
}

TEST_CASE("DiskManager - deallocate and reuse page", "[disk_manager]") {
    TempFile tf(tmpPath("dealloc"));
    DiskManager dm;
    REQUIRE(dm.openFile(4, tf.path));

    PageId p0 = dm.allocatePage(4);
    PageId p1 = dm.allocatePage(4);
    PageId p2 = dm.allocatePage(4);
    REQUIRE(dm.pageCount(4) == 3);

    // Deallocate p1
    dm.deallocatePage(p1);

    // Next allocation reuses p1's slot — no new physical page written
    PageId p3 = dm.allocatePage(4);
    REQUIRE(p3.pageNum() == p1.pageNum());
    REQUIRE(dm.pageCount(4) == 3);  // no extra page appended

    (void)p0; (void)p2;  // suppress unused warnings
}

TEST_CASE("DiskManager - data persists across close and reopen", "[disk_manager]") {
    TempFile tf(tmpPath("persist"));

    // --- Session 1: write data ---
    {
        DiskManager dm;
        REQUIRE(dm.openFile(5, tf.path));
        PageId pid = dm.allocatePage(5);

        char buf[PAGE_SIZE];
        std::memset(buf, 42, PAGE_SIZE);
        REQUIRE(dm.writePage(pid, buf));
        // destructor closes the file
    }

    // --- Session 2: reopen and verify ---
    {
        DiskManager dm;
        REQUIRE(dm.openFile(5, tf.path));
        REQUIRE(dm.pageCount(5) == 1);

        PageId pid(5, 0);
        char buf[PAGE_SIZE];
        std::memset(buf, 0, PAGE_SIZE);
        REQUIRE(dm.readPage(pid, buf));

        char expected[PAGE_SIZE];
        std::memset(expected, 42, PAGE_SIZE);
        REQUIRE(std::memcmp(buf, expected, PAGE_SIZE) == 0);
    }
}

TEST_CASE("DiskManager - multiple files are independent", "[disk_manager]") {
    TempFile tf0(tmpPath("multi0"));
    TempFile tf1(tmpPath("multi1"));
    DiskManager dm;

    REQUIRE(dm.openFile(10, tf0.path));
    REQUIRE(dm.openFile(11, tf1.path));

    dm.allocatePage(10);
    dm.allocatePage(10);
    dm.allocatePage(11);

    REQUIRE(dm.pageCount(10) == 2);
    REQUIRE(dm.pageCount(11) == 1);

    // Write distinct patterns to each file's first page
    char buf10[PAGE_SIZE], buf11[PAGE_SIZE];
    std::memset(buf10, 0xAA, PAGE_SIZE);
    std::memset(buf11, 0xBB, PAGE_SIZE);
    REQUIRE(dm.writePage(PageId(10, 0), buf10));
    REQUIRE(dm.writePage(PageId(11, 0), buf11));

    char r10[PAGE_SIZE], r11[PAGE_SIZE];
    REQUIRE(dm.readPage(PageId(10, 0), r10));
    REQUIRE(dm.readPage(PageId(11, 0), r11));

    REQUIRE(std::memcmp(r10, buf10, PAGE_SIZE) == 0);
    REQUIRE(std::memcmp(r11, buf11, PAGE_SIZE) == 0);
}

TEST_CASE("DiskManager - read out-of-range page returns false", "[disk_manager]") {
    TempFile tf(tmpPath("oob"));
    DiskManager dm;
    REQUIRE(dm.openFile(6, tf.path));

    char buf[PAGE_SIZE];
    // No pages allocated yet
    REQUIRE_FALSE(dm.readPage(PageId(6, 0), buf));
}

TEST_CASE("DiskManager - write multiple pages with independent data", "[disk_manager]") {
    TempFile tf(tmpPath("multi_pages"));
    DiskManager dm;
    REQUIRE(dm.openFile(7, tf.path));

    constexpr int N = 5;
    char write_bufs[N][PAGE_SIZE];
    for (int i = 0; i < N; ++i) {
        dm.allocatePage(7);
        std::memset(write_bufs[i], static_cast<unsigned char>(i + 1), PAGE_SIZE);
        REQUIRE(dm.writePage(PageId(7, static_cast<uint32_t>(i)), write_bufs[i]));
    }

    for (int i = 0; i < N; ++i) {
        char read_buf[PAGE_SIZE];
        REQUIRE(dm.readPage(PageId(7, static_cast<uint32_t>(i)), read_buf));
        REQUIRE(std::memcmp(read_buf, write_bufs[i], PAGE_SIZE) == 0);
    }
}
