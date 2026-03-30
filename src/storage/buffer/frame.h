#ifndef SEEDDB_STORAGE_BUFFER_FRAME_H
#define SEEDDB_STORAGE_BUFFER_FRAME_H

#include "storage/page.h"
#include "storage/page_id.h"

#include <atomic>
#include <cstdint>
#include <shared_mutex>

namespace seeddb {

/// Type alias for a buffer frame index.
using frame_id_t = uint32_t;
constexpr frame_id_t INVALID_FRAME_ID = UINT32_MAX;

/// Lifecycle state of a buffer frame.
enum class FrameState : uint8_t {
    Empty,    ///< Never used
    Loading,  ///< Being loaded from disk (invisible to other threads)
    Ready,    ///< Available for normal use
};

// =============================================================================
// Frame — one slot in the buffer pool
// =============================================================================
//
// A frame holds exactly one page plus its management metadata.
// Frames are stored in a fixed-size array indexed by frame_id_t.
//
// Concurrency model:
//   - pin_count / is_dirty are atomics: safe to read without the global latch.
//   - latch (shared_mutex): protects page *content*.
//     Callers must hold a read or write latch while accessing page data.
//   - All metadata mutations (page_id, state, pin_count) happen under the
//     BufferPool's global mutex (latch_).
// =============================================================================
struct Frame {
    Page                 page;              ///< The actual page data.
    PageId               page_id;           ///< Which page is currently loaded.
    std::atomic<int>     pin_count{0};      ///< Reference count; >0 → not evictable.
    std::atomic<bool>    is_dirty{false};   ///< Modified since last flush?
    FrameState           state{FrameState::Empty}; ///< Lifecycle state.
    std::shared_mutex    latch;             ///< Readers-writer lock for page content.

    // Non-copyable because shared_mutex is non-copyable.
    Frame() = default;
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_BUFFER_FRAME_H
