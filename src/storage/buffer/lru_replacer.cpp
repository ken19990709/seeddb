#include "storage/buffer/lru_replacer.h"

#include <algorithm>
#include <cassert>

namespace seeddb {

LruReplacer::LruReplacer(size_t pool_size, int old_pct)
    : pool_size_(pool_size)
    , target_old_size_(std::max<size_t>(1, (pool_size * static_cast<size_t>(old_pct)) / 100))
{
    // Build sentinel chain: head <-> midpoint <-> tail
    sentinel_head_.next = &midpoint_;
    sentinel_head_.prev = nullptr;
    midpoint_.prev = &sentinel_head_;
    midpoint_.next = &sentinel_tail_;
    sentinel_tail_.prev = &midpoint_;
    sentinel_tail_.next = nullptr;
}

LruReplacer::~LruReplacer() {
    for (auto& [id, node] : nodes_) {
        delete node;
    }
}

void LruReplacer::unlink(Node* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
}

void LruReplacer::insertAfter(Node* pos, Node* node) {
    node->next = pos->next;
    node->prev = pos;
    pos->next->prev = node;
    pos->next = node;
}

void LruReplacer::rebalance() {
    // If old sublist is too large, demote oldest young frame to old head.
    // The oldest young frame is the node immediately before midpoint_.
    while (old_size_ > target_old_size_ && young_size_ > 0) {
        Node* oldest_young = midpoint_.prev;
        if (oldest_young == &sentinel_head_) break;
        unlink(oldest_young);
        --young_size_;
        insertAfter(&midpoint_, oldest_young);
        ++old_size_;
    }
}

void LruReplacer::Pin(frame_id_t frame_id) {
    auto it = nodes_.find(frame_id);
    if (it == nodes_.end()) return;

    Node* node = it->second;
    // Determine which sublist the node is in.
    // A node is in old if it is between midpoint_ and sentinel_tail_.
    // We detect by traversing backwards from sentinel_tail_ — but that is
    // O(n). Instead we track membership via a flag on the node.
    // Simpler: re-traverse the old chain to check.
    // Practical shortcut: compare pointer positions using our size counters
    // is not reliable without a flag. Add an `in_old` bool to Node.
    // → We store this information in a separate set for simplicity.
    // For now: try removing from either sublist by checking size invariants.
    //
    // Actually the cleanest approach: walk the list to determine position.
    // Since this is a buffer pool (pool_size <= thousands), this is acceptable.
    bool in_old = false;
    Node* cur = midpoint_.next;
    while (cur != &sentinel_tail_) {
        if (cur == node) { in_old = true; break; }
        cur = cur->next;
    }

    unlink(node);
    if (in_old) --old_size_;
    else        --young_size_;
}

void LruReplacer::Unpin(frame_id_t frame_id) {
    if (nodes_.count(frame_id)) return;  // already in list

    Node* node = new Node{frame_id, nullptr, nullptr};
    nodes_[frame_id] = node;
    insertAfter(&midpoint_, node);
    ++old_size_;
    // Rebalance: if old is now too big, demote oldest young to old.
    // (Only happens when young is non-empty.)
    rebalance();
}

void LruReplacer::Access(frame_id_t frame_id) {
    auto it = nodes_.find(frame_id);
    if (it == nodes_.end()) return;  // pinned or not in pool

    Node* node = it->second;

    // Determine if in old sublist.
    bool in_old = false;
    Node* cur = midpoint_.next;
    while (cur != &sentinel_tail_) {
        if (cur == node) { in_old = true; break; }
        cur = cur->next;
    }

    unlink(node);
    if (in_old) {
        --old_size_;
        insertAfter(&sentinel_head_, node);
        ++young_size_;
        // Rebalance: young grew, may need to demote an old-young boundary frame.
        rebalance();
    } else {
        --young_size_;
        insertAfter(&sentinel_head_, node);
        ++young_size_;
    }
}

bool LruReplacer::Evict(frame_id_t* frame_id) {
    // Evict from old tail.
    Node* victim = sentinel_tail_.prev;
    if (victim == &midpoint_ || victim == &sentinel_head_) {
        return false;  // no evictable frames
    }
    *frame_id = victim->frame_id;
    unlink(victim);
    --old_size_;
    nodes_.erase(victim->frame_id);
    delete victim;
    return true;
}

size_t LruReplacer::Size() const {
    return young_size_ + old_size_;
}

} // namespace seeddb
