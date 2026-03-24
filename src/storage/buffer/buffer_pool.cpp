#include "storage/buffer/buffer_pool.h"
#include "storage/page.h"

#include <cassert>
#include <stdexcept>

namespace seeddb {

BufferPool::BufferPool(PageManager& page_manager, const Config& config)
    : page_manager_(page_manager)
    , pool_size_(static_cast<size_t>(config.buffer_pool_size()))
    , frames_(pool_size_)
    , replacer_(pool_size_, config.buffer_pool_old_pct())
{
    if (pool_size_ == 0) {
        throw std::invalid_argument("BufferPool: pool_size must be >= 1");
    }
}

BufferPool::~BufferPool() {
    FlushAll();
}

Page* BufferPool::FetchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);

    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        // Cache hit
        frame_id_t fid = it->second;
        frames_[fid].pin_count.fetch_add(1, std::memory_order_relaxed);
        replacer_.Access(fid);
        return &frames_[fid].page;
    }

    // Cache miss: find a victim frame
    frame_id_t fid = findVictim();
    if (fid == INVALID_FRAME_ID) return nullptr;

    // Load the page from disk (releases latch_ during I/O)
    loadPage(fid, page_id);

    page_table_[page_id] = fid;
    frames_[fid].page_id  = page_id;
    frames_[fid].valid    = true;
    frames_[fid].pin_count.store(1, std::memory_order_relaxed);
    frames_[fid].is_dirty.store(false, std::memory_order_relaxed);
    replacer_.Pin(fid);

    return &frames_[fid].page;
}

bool BufferPool::UnpinPage(PageId page_id, bool is_dirty) {
    std::unique_lock<std::mutex> guard(latch_);

    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t fid = it->second;
    int prev = frames_[fid].pin_count.fetch_sub(1, std::memory_order_relaxed);
    if (prev <= 0) {
        frames_[fid].pin_count.store(0, std::memory_order_relaxed);
        return false;
    }
    if (is_dirty) frames_[fid].is_dirty.store(true, std::memory_order_relaxed);
    if (prev == 1) {
        // pin_count dropped to 0: make evictable
        replacer_.Unpin(fid);
    }
    return true;
}

bool BufferPool::FlushPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;
    flushFrame(it->second);
    return true;
}

void BufferPool::FlushAll() {
    std::unique_lock<std::mutex> guard(latch_);
    for (frame_id_t fid = 0; fid < pool_size_; ++fid) {
        if (frames_[fid].valid && frames_[fid].is_dirty.load()) {
            flushFrame(fid);
        }
    }
}

// --- Latch API ---------------------------------------------------------------

void BufferPool::RLatchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    frame_id_t fid = it->second;
    guard.unlock();
    frames_[fid].latch.lock_shared();
}

void BufferPool::RUnlatchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    frame_id_t fid = it->second;
    guard.unlock();
    frames_[fid].latch.unlock_shared();
}

void BufferPool::WLatchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    frame_id_t fid = it->second;
    guard.unlock();
    frames_[fid].latch.lock();
}

void BufferPool::WUnlatchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    frame_id_t fid = it->second;
    guard.unlock();
    frames_[fid].latch.unlock();
}

// --- Diagnostics -------------------------------------------------------------

size_t BufferPool::poolSize() const { return pool_size_; }

size_t BufferPool::pinnedCount() const {
    std::unique_lock<std::mutex> guard(latch_);
    size_t count = 0;
    for (const auto& f : frames_) {
        if (f.valid && f.pin_count.load() > 0) ++count;
    }
    return count;
}

// --- Private helpers ---------------------------------------------------------

frame_id_t BufferPool::findVictim() {
    // First: look for an unused frame (never loaded)
    for (frame_id_t fid = 0; fid < pool_size_; ++fid) {
        if (!frames_[fid].valid) return fid;
    }
    // Second: ask replacer for an eviction candidate
    frame_id_t fid;
    if (!replacer_.Evict(&fid)) return INVALID_FRAME_ID;

    // Evict: flush if dirty, remove from page table
    flushFrame(fid);
    page_table_.erase(frames_[fid].page_id);
    frames_[fid].valid = false;
    return fid;
}

void BufferPool::loadPage(frame_id_t frame_id, PageId page_id) {
    // Release global latch during disk I/O to avoid blocking other threads.
    latch_.unlock();
    frames_[frame_id].latch.lock();  // exclusive: writing page content
    page_manager_.getPage(page_id, frames_[frame_id].page);
    frames_[frame_id].latch.unlock();
    latch_.lock();
}

void BufferPool::flushFrame(frame_id_t frame_id) {
    Frame& f = frames_[frame_id];
    if (!f.valid || !f.is_dirty.load()) return;
    // Release global latch during disk I/O
    latch_.unlock();
    f.latch.lock();
    page_manager_.writePage(f.page_id, f.page);
    f.latch.unlock();
    latch_.lock();
    f.is_dirty.store(false, std::memory_order_relaxed);
}

} // namespace seeddb
