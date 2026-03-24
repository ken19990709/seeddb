#ifndef SEEDDB_STORAGE_BUFFER_LRU_REPLACER_H
#define SEEDDB_STORAGE_BUFFER_LRU_REPLACER_H

#include "storage/buffer/frame.h"

#include <cstddef>
#include <unordered_map>

namespace seeddb {

// =============================================================================
// LruReplacer — LRU list with midpoint insertion (InnoDB style)
// =============================================================================
//
// The list is a doubly-linked list divided into young (hot) and old (cold)
// sublists by a sentinel midpoint_ node:
//
//   sentinel_head_ <-> [young] <-> midpoint_ <-> [old] <-> sentinel_tail_
//
// New frames enter at midpoint (head of old sublist).
// Accessed frames are promoted to young head.
// Eviction always takes from old tail.
// Rebalancing: if old_size_ > target_old_size_, the oldest young frame is
//   demoted to old head to maintain the split ratio.
//
// Thread safety: NOT thread-safe internally. The BufferPool's global mutex
//   must be held by the caller during all LruReplacer operations.
// =============================================================================
class LruReplacer {
public:
    /// @param pool_size  Total number of frames managed.
    /// @param old_pct    Percentage of frames in the old sublist (0–100).
    explicit LruReplacer(size_t pool_size, int old_pct);
    ~LruReplacer();

    LruReplacer(const LruReplacer&)            = delete;
    LruReplacer& operator=(const LruReplacer&) = delete;

    /// Remove @p frame_id from the LRU list (frame is now pinned).
    void Pin(frame_id_t frame_id);

    /// Insert @p frame_id at midpoint (head of old sublist).
    /// If the frame is already in the list, this is a no-op.
    void Unpin(frame_id_t frame_id);

    /// Record an access to @p frame_id.
    /// - If in old sublist: promote to young head.
    /// - If in young sublist: move to young head.
    /// - If not in list (pinned): no-op.
    void Access(frame_id_t frame_id);

    /// Choose a victim from the old tail.
    /// @param[out] frame_id  The evicted frame id.
    /// @return false if all frames are pinned (no candidates).
    bool Evict(frame_id_t* frame_id);

    /// Number of frames currently eligible for eviction.
    size_t Size() const;

private:
    struct Node {
        frame_id_t frame_id{INVALID_FRAME_ID};
        Node* prev{nullptr};
        Node* next{nullptr};
    };

    /// Unlinks @p node from the list (does not delete).
    void unlink(Node* node);

    /// Inserts @p node immediately after @p pos.
    void insertAfter(Node* pos, Node* node);

    /// If old_size_ > target_old_size_, demote the oldest young frame.
    void rebalance();

    Node sentinel_head_;   ///< Dummy head (before all young frames).
    Node sentinel_tail_;   ///< Dummy tail (after all old frames).
    Node midpoint_;        ///< Boundary sentinel between young and old.

    std::unordered_map<frame_id_t, Node*> nodes_;  ///< frame_id → list node.

    size_t pool_size_;
    size_t target_old_size_;   ///< Desired number of old-sublist frames.
    size_t young_size_{0};
    size_t old_size_{0};
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_BUFFER_LRU_REPLACER_H
