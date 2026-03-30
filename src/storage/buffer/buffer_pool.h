#ifndef SEEDDB_STORAGE_BUFFER_BUFFER_POOL_H
#define SEEDDB_STORAGE_BUFFER_BUFFER_POOL_H

#include "storage/buffer/frame.h"
#include "storage/buffer/lru_replacer.h"
#include "storage/page_manager.h"
#include "common/config.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace seeddb {

// =============================================================================
// BufferPool — in-memory page cache
// =============================================================================
//
// Sits between the execution engine and disk.  Maintains a fixed-size array
// of Frames and a page table (PageId → frame_id_t) for O(1) lookups.
//
// Eviction policy: LRU with midpoint insertion (InnoDB style).
//
// Concurrency:
//   latch_      — global std::mutex; protects page_table_, replacer_, and
//                 Frame metadata (page_id, valid, pin_count).
//   Frame.latch — per-frame std::shared_mutex; protects page *content*.
//                 Callers acquire via RLatchPage / WLatchPage after FetchPage.
//                 The global latch_ is NOT held while a frame latch is held.
// =============================================================================
class BufferPool {
public:
    /// Constructs pool.  Reads buffer_pool_size and buffer_pool_old_pct from
    /// @p config.  Pool size must be >= 1.
    explicit BufferPool(PageManager& page_manager, const Config& config);

    /// Flushes all dirty pages on destruction.
    ~BufferPool();

    BufferPool(const BufferPool&)            = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    // =========================================================================
    // Core page lifecycle
    // =========================================================================

    /// Fetch @p page_id into memory (pins it, increments pin_count).
    /// Cache hit: returns existing frame, calls Access().
    /// Cache miss: evicts a victim if needed, loads from disk.
    /// Returns nullptr if all frames are pinned (pool exhausted).
    Page* FetchPage(PageId page_id);

    /// Decrement pin_count for @p page_id.
    /// If is_dirty is true, sets frame.is_dirty = true.
    /// If pin_count drops to 0, makes frame eligible for eviction.
    /// Returns false if the page is not in the pool.
    bool UnpinPage(PageId page_id, bool is_dirty);

    /// Flush a dirty page to disk without evicting it.
    /// Returns false if the page is not in the pool.
    bool FlushPage(PageId page_id);

    /// Flush all dirty pages (used at checkpoint / shutdown).
    void FlushAll();

    // =========================================================================
    // Latch API
    // =========================================================================
    // The page must be pinned (FetchPage called) before acquiring a latch.
    // Callers are responsible for releasing latches before calling UnpinPage.

    void RLatchPage(PageId page_id);    ///< Acquire shared latch (multiple readers OK).
    void RUnlatchPage(PageId page_id);  ///< Release shared latch.
    void WLatchPage(PageId page_id);    ///< Acquire exclusive latch.
    void WUnlatchPage(PageId page_id);  ///< Release exclusive latch.

    // =========================================================================
    // Diagnostics
    // =========================================================================

    /// Total number of frames in the pool.
    size_t poolSize() const;

    /// Number of currently pinned pages (pin_count > 0).
    size_t pinnedCount() const;

private:
    // =========================================================================
    // Internal helpers
    // =========================================================================

    /// Find a free frame (unused or evictable).
    /// Returns INVALID_FRAME_ID if the pool is full and all frames are pinned.
    /// Flushes the evicted frame if dirty. Removes it from page_table_.
    frame_id_t findVictim();

    /// Load @p page_id from disk into @p frame_id.
    /// Acquires write latch on the frame during I/O.
    /// Caller must hold latch_ when entering; latch_ is released during I/O.
    /// @return true on success, false on I/O error.
    bool loadPage(frame_id_t frame_id, PageId page_id);

    /// Flush the frame to disk if dirty.  Caller must hold latch_.
    void flushFrame(frame_id_t frame_id);

    // =========================================================================
    // Data
    // =========================================================================

    PageManager&    page_manager_;
    const size_t    pool_size_;

    std::vector<Frame>                          frames_;      ///< frame_id → Frame
    std::unordered_map<PageId, frame_id_t>      page_table_;  ///< PageId → frame_id
    LruReplacer                                 replacer_;
    mutable std::mutex                          latch_;       ///< global metadata lock
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_BUFFER_BUFFER_POOL_H
