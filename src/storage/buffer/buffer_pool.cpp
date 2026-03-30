#include "storage/buffer/buffer_pool.h"

#include <stdexcept>
#include <utility>

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
    try { FlushAll(); } catch (...) {}
}

Page* BufferPool::FetchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);

    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        frame_id_t fid = it->second;
        // If still loading, wait until ready
        while (frames_[fid].state == FrameState::Loading) {
            guard.unlock();
            // Spin briefly; the loading thread holds a pin so eviction can't steal it.
            frames_[fid].latch.lock_shared();
            frames_[fid].latch.unlock_shared();
            guard.lock();
            // Re-validate: page_table_ entry may have changed
            it = page_table_.find(page_id);
            if (it == page_table_.end()) break;
            fid = it->second;
        }
        if (it != page_table_.end() && frames_[fid].state == FrameState::Ready) {
            frames_[fid].pin_count.fetch_add(1, std::memory_order_relaxed);
            replacer_.Access(fid);
            return &frames_[fid].page;
        }
        // Loading failed; fall through to cache miss
    }

    // Cache miss: find a victim frame
    frame_id_t fid = findVictim();
    if (fid == INVALID_FRAME_ID) return nullptr;

    // Mark frame as Loading (invisible to cache hits, prevents eviction)
    frames_[fid].page_id  = page_id;
    frames_[fid].state    = FrameState::Loading;
    frames_[fid].pin_count.store(1, std::memory_order_relaxed);
    frames_[fid].is_dirty.store(false, std::memory_order_relaxed);
    replacer_.Pin(fid);

    // Load the page from disk (releases latch_ during I/O)
    bool ok = loadPage(fid, page_id);

    if (!ok) {
        // I/O failed: reset frame, do NOT insert into page_table_
        frames_[fid].state    = FrameState::Empty;
        frames_[fid].pin_count.store(0, std::memory_order_relaxed);
        return nullptr;
    }

    frames_[fid].state = FrameState::Ready;
    page_table_[page_id] = fid;

    return &frames_[fid].page;
}

bool BufferPool::UnpinPage(PageId page_id, bool is_dirty) {
    std::unique_lock<std::mutex> guard(latch_);

    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t fid = it->second;
    int expected = frames_[fid].pin_count.load(std::memory_order_relaxed);
    while (expected > 0) {
        if (frames_[fid].pin_count.compare_exchange_weak(
                expected, expected - 1, std::memory_order_relaxed)) {
            break;
        }
    }
    if (expected <= 0) return false;

    if (is_dirty) frames_[fid].is_dirty.store(true, std::memory_order_relaxed);
    if (expected == 1) {
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
    // Snapshot dirty frames to avoid iterating during latch releases
    std::vector<std::pair<frame_id_t, PageId>> dirty_frames;
    for (frame_id_t fid = 0; fid < pool_size_; ++fid) {
        if (frames_[fid].state == FrameState::Ready && frames_[fid].is_dirty.load()) {
            dirty_frames.emplace_back(fid, frames_[fid].page_id);
        }
    }
    for (auto [fid, pid] : dirty_frames) {
        // Verify frame still belongs to same page before flushing
        if (frames_[fid].state == FrameState::Ready && frames_[fid].page_id == pid) {
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
    frames_[fid].pin_count.fetch_add(1, std::memory_order_relaxed);
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
    int prev = frames_[fid].pin_count.fetch_sub(1, std::memory_order_relaxed);
    if (prev == 1) {
        guard.lock();
        replacer_.Unpin(fid);
    }
}

void BufferPool::WLatchPage(PageId page_id) {
    std::unique_lock<std::mutex> guard(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    frame_id_t fid = it->second;
    frames_[fid].pin_count.fetch_add(1, std::memory_order_relaxed);
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
    int prev = frames_[fid].pin_count.fetch_sub(1, std::memory_order_relaxed);
    if (prev == 1) {
        guard.lock();
        replacer_.Unpin(fid);
    }
}

// --- Diagnostics -------------------------------------------------------------

size_t BufferPool::poolSize() const { return pool_size_; }

size_t BufferPool::pinnedCount() const {
    std::unique_lock<std::mutex> guard(latch_);
    size_t count = 0;
    for (const auto& f : frames_) {
        if (f.state == FrameState::Ready && f.pin_count.load() > 0) ++count;
    }
    return count;
}

// --- Private helpers ---------------------------------------------------------

frame_id_t BufferPool::findVictim() {
    // First: look for an unused frame (never loaded)
    for (frame_id_t fid = 0; fid < pool_size_; ++fid) {
        if (frames_[fid].state == FrameState::Empty) return fid;
    }
    // Second: ask replacer for an eviction candidate
    frame_id_t fid;
    if (!replacer_.Evict(&fid)) return INVALID_FRAME_ID;

    // Evict: flush if dirty, remove from page table
    flushFrame(fid);
    page_table_.erase(frames_[fid].page_id);
    frames_[fid].state = FrameState::Empty;
    return fid;
}

bool BufferPool::loadPage(frame_id_t frame_id, PageId page_id) {
    // Release global latch during disk I/O to avoid blocking other threads.
    latch_.unlock();
    bool ok = false;
    try {
        std::unique_lock<std::shared_mutex> frame_guard(frames_[frame_id].latch);
        ok = page_manager_.getPage(page_id, frames_[frame_id].page);
    } catch (...) {
        latch_.lock();
        throw;
    }
    latch_.lock();
    return ok;
}

void BufferPool::flushFrame(frame_id_t frame_id) {
    Frame& f = frames_[frame_id];
    if (f.state != FrameState::Ready || !f.is_dirty.load()) return;
    // Swap out the dirty flag while we still hold the global latch
    f.is_dirty.store(false, std::memory_order_relaxed);
    // Release global latch during disk I/O
    PageId pid = f.page_id;
    latch_.unlock();
    bool ok = false;
    try {
        std::unique_lock<std::shared_mutex> frame_guard(f.latch);
        ok = page_manager_.writePage(pid, f.page);
    } catch (...) {
        latch_.lock();
        f.is_dirty.store(true, std::memory_order_relaxed);
        throw;
    }
    latch_.lock();
    if (!ok) {
        // I/O failed: restore dirty flag so we retry later
        f.is_dirty.store(true, std::memory_order_relaxed);
    }
}

} // namespace seeddb
