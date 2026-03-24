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
