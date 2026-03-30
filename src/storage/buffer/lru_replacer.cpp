#include "storage/buffer/lru_replacer.h"

#include <algorithm>

namespace seeddb {

LruReplacer::LruReplacer(size_t pool_size, int old_pct)
    : pool_size_(pool_size)
    , target_old_size_(std::max<size_t>(1, (pool_size * static_cast<size_t>(std::clamp(old_pct, 5, 95))) / 100))
{
    // Build sentinel chain: head <-> midpoint <-> tail
    sentinel_head_.next = &midpoint_;
    sentinel_head_.prev = nullptr;
    midpoint_.prev = &sentinel_head_;
    midpoint_.next = &sentinel_tail_;
    sentinel_tail_.prev = &midpoint_;
    sentinel_tail_.next = nullptr;
}

LruReplacer::~LruReplacer() = default;

void LruReplacer::unlink(Node* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
}

void LruReplacer::insertAfter(Node* pos, Node* node, bool mark_old) {
    node->next = pos->next;
    node->prev = pos;
    pos->next->prev = node;
    pos->next = node;
    node->in_old = mark_old;
}

void LruReplacer::rebalance() {
    // If old sublist is too large, demote oldest young frame to old head.
    while (old_size_ > target_old_size_ && young_size_ > 0) {
        Node* oldest_young = midpoint_.prev;
        if (oldest_young == &sentinel_head_) break;
        unlink(oldest_young);
        --young_size_;
        insertAfter(&midpoint_, oldest_young, true);
        ++old_size_;
    }
}

void LruReplacer::Pin(frame_id_t frame_id) {
    auto it = nodes_.find(frame_id);
    if (it == nodes_.end()) return;

    Node* node = it->second.get();
    bool in_old = node->in_old;
    unlink(node);
    if (in_old) --old_size_;
    else        --young_size_;
}

void LruReplacer::Unpin(frame_id_t frame_id) {
    if (nodes_.count(frame_id)) return;  // already in list

    auto node = std::make_unique<Node>();
    node->frame_id = frame_id;
    Node* raw = node.get();
    nodes_[frame_id] = std::move(node);
    insertAfter(&midpoint_, raw, true);
    ++old_size_;
    rebalance();
}

void LruReplacer::Access(frame_id_t frame_id) {
    auto it = nodes_.find(frame_id);
    if (it == nodes_.end()) return;  // pinned or not in pool

    Node* node = it->second.get();

    unlink(node);
    if (node->in_old) {
        --old_size_;
        insertAfter(&sentinel_head_, node, false);
        ++young_size_;
        rebalance();
    } else {
        // young: move to young head, size unchanged
        insertAfter(&sentinel_head_, node, false);
    }
}

bool LruReplacer::Evict(frame_id_t* frame_id) {
    // Evict from old tail.
    Node* victim = sentinel_tail_.prev;
    if (victim != &midpoint_ && victim != &sentinel_head_) {
        *frame_id = victim->frame_id;
        unlink(victim);
        --old_size_;
        nodes_.erase(victim->frame_id);
        return true;
    }
    // Fallback: evict from young tail
    Node* young_tail = midpoint_.prev;
    if (young_tail != &sentinel_head_) {
        *frame_id = young_tail->frame_id;
        unlink(young_tail);
        --young_size_;
        nodes_.erase(young_tail->frame_id);
        return true;
    }
    return false;
}

size_t LruReplacer::Size() const {
    return young_size_ + old_size_;
}

} // namespace seeddb
