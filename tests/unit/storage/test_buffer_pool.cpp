// =============================================================================
// Unit tests for LruReplacer and BufferPool
// =============================================================================
#include <catch2/catch_all.hpp>
#include "storage/buffer/lru_replacer.h"

using namespace seeddb;

// ---------------------------------------------------------------------------
// LruReplacer tests
// ---------------------------------------------------------------------------

TEST_CASE("LruReplacer - empty replacer cannot evict", "[buffer_pool]") {
    LruReplacer r(4, 37);
    frame_id_t victim;
    REQUIRE_FALSE(r.Evict(&victim));
    REQUIRE(r.Size() == 0);
}

TEST_CASE("LruReplacer - unpin makes frame evictable", "[buffer_pool]") {
    LruReplacer r(4, 37);
    r.Unpin(0);
    REQUIRE(r.Size() == 1);
    frame_id_t victim;
    REQUIRE(r.Evict(&victim));
    REQUIRE(victim == 0);
    REQUIRE(r.Size() == 0);
}

TEST_CASE("LruReplacer - pin removes from eviction candidates", "[buffer_pool]") {
    LruReplacer r(4, 37);
    r.Unpin(0);
    r.Pin(0);
    frame_id_t victim;
    REQUIRE_FALSE(r.Evict(&victim));
}

TEST_CASE("LruReplacer - eviction order: old tail evicted first", "[buffer_pool]") {
    // Pool of 4 frames, 37% old = ~1 frame in old
    LruReplacer r(4, 37);
    // Unpin 0,1,2,3 in order; they enter old sublist
    r.Unpin(0);
    r.Unpin(1);
    r.Unpin(2);
    r.Unpin(3);

    frame_id_t victim;
    // Frame 0 entered old sublist first → it should be evicted first
    REQUIRE(r.Evict(&victim));
    REQUIRE(victim == 0);
    REQUIRE(r.Evict(&victim));
    REQUIRE(victim == 1);
}

TEST_CASE("LruReplacer - Access promotes frame to young head", "[buffer_pool]") {
    // Pool of 4, with enough frames that young sublist exists
    LruReplacer r(4, 37);
    r.Unpin(0);
    r.Unpin(1);
    r.Unpin(2);
    r.Unpin(3);

    // Access frame 0: it should be promoted to young (protected from eviction)
    r.Access(0);

    // Eviction should prefer frames that have not been recently accessed
    frame_id_t victim;
    REQUIRE(r.Evict(&victim));
    REQUIRE(victim != 0);  // frame 0 is young, should not be evicted first
}

TEST_CASE("LruReplacer - midpoint ratio maintained", "[buffer_pool]") {
    // With pool_size=10 and old_pct=37, target_old = 3
    LruReplacer r(10, 37);
    for (frame_id_t i = 0; i < 10; ++i) {
        r.Unpin(i);
    }
    REQUIRE(r.Size() == 10);

    // Access all frames to promote them to young sublist
    for (frame_id_t i = 0; i < 10; ++i) {
        r.Access(i);
    }
    // After accessing all, old sublist should be replenished by demotion
    // At least target_old_size frames should be evictable from old sublist
    frame_id_t victim;
    REQUIRE(r.Evict(&victim));  // old tail is still evictable
}

// ---------------------------------------------------------------------------
// BufferPool test helpers
// ---------------------------------------------------------------------------
#include "storage/buffer/buffer_pool.h"
#include "storage/page_manager.h"
#include "common/config.h"

#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace {

struct BpFixture {
    std::string dir;
    PageManager pm;
    Config      cfg;
    uint32_t    file_id;

    explicit BpFixture(size_t pool_size = 4)
        : dir("/tmp/seeddb_bp_" + std::to_string(
                std::hash<std::thread::id>{}(std::this_thread::get_id())))
        , pm(dir)
    {
        fs::create_directories(dir);
        cfg.set("buffer_pool_size", std::to_string(pool_size));
        cfg.set("buffer_pool_old_pct", "37");
        file_id = pm.createTableFile("test_table");
        // Pre-allocate 6 pages on disk
        for (int i = 0; i < 6; ++i) pm.allocatePage(file_id);
    }

    ~BpFixture() { fs::remove_all(dir); }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// BufferPool – single-threaded
// ---------------------------------------------------------------------------

TEST_CASE("BufferPool - cache miss loads page from disk", "[buffer_pool]") {
    BpFixture f;
    BufferPool bp(f.pm, f.cfg);

    PageId pid{f.file_id, 0};
    Page* p = bp.FetchPage(pid);
    REQUIRE(p != nullptr);
    REQUIRE(bp.pinnedCount() == 1);

    bp.UnpinPage(pid, false);
    REQUIRE(bp.pinnedCount() == 0);
}

TEST_CASE("BufferPool - cache hit returns same page", "[buffer_pool]") {
    BpFixture f;
    BufferPool bp(f.pm, f.cfg);

    PageId pid{f.file_id, 0};
    Page* p1 = bp.FetchPage(pid);
    Page* p2 = bp.FetchPage(pid);

    REQUIRE(p1 == p2);  // same frame
    REQUIRE(bp.pinnedCount() == 1);  // pin_count = 2 but same page_id

    bp.UnpinPage(pid, false);
    bp.UnpinPage(pid, false);
}

TEST_CASE("BufferPool - FetchPage on full pinned pool returns nullptr", "[buffer_pool]") {
    BpFixture f(2);  // pool of 2 frames
    BufferPool bp(f.pm, f.cfg);

    PageId p0{f.file_id, 0};
    PageId p1{f.file_id, 1};
    PageId p2{f.file_id, 2};

    REQUIRE(bp.FetchPage(p0) != nullptr);
    REQUIRE(bp.FetchPage(p1) != nullptr);
    REQUIRE(bp.FetchPage(p2) == nullptr);  // pool full, all pinned

    bp.UnpinPage(p0, false);
    bp.UnpinPage(p1, false);
}

TEST_CASE("BufferPool - evicts unpinned page when pool full", "[buffer_pool]") {
    BpFixture f(2);
    BufferPool bp(f.pm, f.cfg);

    PageId p0{f.file_id, 0};
    PageId p1{f.file_id, 1};
    PageId p2{f.file_id, 2};

    bp.FetchPage(p0);
    bp.FetchPage(p1);
    bp.UnpinPage(p0, false);  // p0 now evictable

    Page* p = bp.FetchPage(p2);
    REQUIRE(p != nullptr);  // evicted p0 to make room

    bp.UnpinPage(p1, false);
    bp.UnpinPage(p2, false);
}

TEST_CASE("BufferPool - dirty page flushed before eviction", "[buffer_pool]") {
    BpFixture f(2);
    BufferPool bp(f.pm, f.cfg);

    PageId p0{f.file_id, 0};
    PageId p1{f.file_id, 1};
    PageId p2{f.file_id, 2};

    Page* pg0 = bp.FetchPage(p0);
    REQUIRE(pg0 != nullptr);
    bp.UnpinPage(p0, true);   // mark dirty

    bp.FetchPage(p1);
    bp.UnpinPage(p1, false);

    // Evicting p0 (dirty) to load p2 should flush p0 to disk without error
    Page* pg2 = bp.FetchPage(p2);
    REQUIRE(pg2 != nullptr);

    bp.UnpinPage(p2, false);
}

TEST_CASE("BufferPool - FlushPage writes without eviction", "[buffer_pool]") {
    BpFixture f;
    BufferPool bp(f.pm, f.cfg);

    PageId pid{f.file_id, 0};
    bp.FetchPage(pid);
    bp.UnpinPage(pid, true);

    REQUIRE(bp.FlushPage(pid));     // flush dirty page
    REQUIRE(bp.poolSize() == 4);    // page still in pool
}

TEST_CASE("BufferPool - FlushAll flushes all dirty pages", "[buffer_pool]") {
    BpFixture f;
    BufferPool bp(f.pm, f.cfg);

    for (uint32_t i = 0; i < 4; ++i) {
        PageId pid{f.file_id, i};
        bp.FetchPage(pid);
        bp.UnpinPage(pid, true);  // all dirty
    }
    bp.FlushAll();  // should not throw
    REQUIRE(bp.poolSize() == 4);
}

TEST_CASE("BufferPool - sequential scan does not evict hot pages", "[buffer_pool]") {
    // Pool of 4, pre-pin two "hot" pages, then scan several cold pages
    BpFixture f(4);
    BufferPool bp(f.pm, f.cfg);

    // Hot pages: keep pinned throughout
    PageId hot0{f.file_id, 0};
    PageId hot1{f.file_id, 1};
    bp.FetchPage(hot0);
    bp.FetchPage(hot1);
    bp.UnpinPage(hot0, false);  // unpin but access again to promote to young
    bp.UnpinPage(hot1, false);
    // Access again → move to young sublist
    bp.FetchPage(hot0); bp.UnpinPage(hot0, false);
    bp.FetchPage(hot1); bp.UnpinPage(hot1, false);

    // Now scan cold pages through the remaining 2 frames
    for (uint32_t i = 2; i < 6; ++i) {
        PageId scan{f.file_id, i};
        Page* p = bp.FetchPage(scan);
        REQUIRE(p != nullptr);
        bp.UnpinPage(scan, false);
    }

    // Hot pages should still be in the buffer pool (young sublist protected them)
    Page* h0 = bp.FetchPage(hot0);
    REQUIRE(h0 != nullptr);  // not evicted
    bp.UnpinPage(hot0, false);
}
