# Buffer Pool Design Specification

**Date**: 2026-03-24
**Phase**: 3.2 (Buffer Pool)
**Status**: Design Complete

## Overview

This document specifies the design for SeedDB's Buffer Pool — the in-memory page cache that sits between the execution engine and the disk. The Buffer Pool manages a fixed set of frames (configurable at runtime), loads pages from disk on demand, and evicts cold pages using an LRU-with-midpoint-insertion policy.

## Design Decisions

### 1. Pool Size: Runtime-Configurable via Config File

**Rationale**:
- Buffer pool size directly impacts performance; tuning without recompilation is essential
- Config infrastructure already exists (`Config` class, `seeddb.conf`)
- `buffer_pool_size` key already declared in `config.h` (default 1024 frames)

**Configuration keys in `seeddb.conf`**:
```ini
buffer_pool_size = 1024   # Number of frames (1024 × 4KB = 4MB default)
buffer_pool_old_pct = 37  # Percentage allocated to old sublist (InnoDB default)
```

### 2. Replacement Policy: LRU with Midpoint Insertion (InnoDB Style)

**Rationale**:
- Pure LRU is susceptible to sequential flooding: a full table scan can evict all hot pages
- Midpoint insertion divides the LRU list into a *young* (hot) sublist and an *old* (cold) sublist
- New pages enter at the midpoint (head of old sublist), protecting young pages from scan pollution
- Pages re-accessed while in the old sublist are promoted to the young head
- Eviction always pulls from the old tail
- Default split: 63% young / 37% old (matching MySQL InnoDB defaults)

**LRU list layout**:
```
[young head] → Y → Y → Y → [midpoint] → O → O → O → [old tail]
                                                          ↑ evict here
```

### 3. Latching: Full Reader-Writer Latch per Frame (from the start)

**Rationale**:
- Concurrent access control is fundamental to a correct buffer pool
- Each frame carries a `std::shared_mutex` that permits multiple concurrent readers or one exclusive writer
- A global mutex protects metadata operations (page table, LRU list, frame assignment)
- This matches PostgreSQL's buffer latch model

**Distinction between pin and latch**:
- **Pin (pin_count)**: prevents eviction — held for the duration of a query operation on a page
- **Latch**: controls read/write access to page content — held only during the actual read or write

### 4. API Design: Pointer-Based FetchPage/UnpinPage (PostgreSQL/InnoDB Style)

**Rationale**:
- Matches the architectural model of both PostgreSQL and InnoDB
- Clear ownership semantics: caller pins, uses, then unpins
- Straightforward to reason about lifetimes during development
- Can be wrapped in RAII handles later without changing the core interface

---

## Component Design

### Component 1: Frame (`frame.h`)

A Frame is the in-memory slot that holds one page plus its management state:

```cpp
using frame_id_t = uint32_t;
constexpr frame_id_t INVALID_FRAME_ID = UINT32_MAX;

struct Frame {
    Page                 page;           // The actual page data
    PageId               page_id;        // Which page is currently loaded
    std::atomic<int>     pin_count{0};   // Reference count; >0 means not evictable
    std::atomic<bool>    is_dirty{false};// True if page was modified since last flush
    bool                 valid{false};   // True if a page is loaded into this frame
    std::shared_mutex    latch;          // Reader-writer lock for page content access
};
```

### Component 2: LruReplacer (`lru_replacer.h/.cpp`)

Maintains the LRU list with midpoint insertion. Operates on `frame_id_t` values.

```
Internal state:
  - Doubly-linked list node for each frame
  - Two sentinel nodes: young_head_, old_tail_
  - midpoint_ pointer: the boundary between young and old sublists
  - current_old_size_ / current_young_size_ counters
  - target_old_size_ = (pool_size * old_pct) / 100
```

**Key methods**:

| Method | Behavior |
|--------|----------|
| `void Pin(frame_id_t)` | Remove frame from LRU list (currently pinned, cannot be evicted) |
| `void Unpin(frame_id_t)` | Insert frame at midpoint (head of old sublist) if not in list |
| `void Access(frame_id_t)` | Move frame to young head; if in old list, promote to young |
| `bool Evict(frame_id_t*)` | Remove and return the old tail (oldest unpinned frame); return false if none available |
| `size_t Size() const` | Number of frames currently eligible for eviction |

**LRU midpoint rebalancing**: After every promotion or insertion, if the old sublist exceeds `target_old_size_`, the oldest young frame is demoted to old head. This keeps the split ratio stable.

### Component 3: BufferPool (`buffer_pool.h/.cpp`)

The main class. Owns the frame array, page table, and LRU replacer.

```
Internal state:
  - std::vector<Frame>  frames_          // Fixed-size array; index = frame_id
  - std::unordered_map<PageId, frame_id_t> page_table_   // PageId → frame slot
  - LruReplacer         replacer_
  - std::mutex          latch_           // Protects page_table_ and replacer_
  - PageManager&        page_manager_    // For disk I/O
  - size_t              pool_size_
```

**Public API**:

```cpp
class BufferPool {
public:
    // Constructs pool; reads buffer_pool_size and buffer_pool_old_pct from config
    explicit BufferPool(PageManager& page_manager, const Config& config);
    ~BufferPool();  // Calls FlushAll()

    // Fetch page into memory.
    // - Cache hit:  increment pin_count, call Access(), return &frame.page
    // - Cache miss: find a victim via Evict(), flush if dirty, load from disk,
    //               insert into page_table_, pin, return &frame.page
    // - Returns nullptr if all frames are pinned (pool full)
    Page* FetchPage(PageId page_id);

    // Decrement pin_count for page_id.
    // If is_dirty, set frame.is_dirty = true.
    // If pin_count drops to 0, call replacer_.Unpin(frame_id).
    // Returns false if page not in pool.
    bool UnpinPage(PageId page_id, bool is_dirty);

    // Write a dirty page to disk immediately; does NOT evict the frame.
    // Returns false if page not in pool.
    bool FlushPage(PageId page_id);

    // Flush all dirty pages to disk (used at checkpoint / shutdown).
    void FlushAll();

    // --- Latch API (page must be pinned before latching) ---
    void RLatchPage(PageId page_id);    // shared_lock on frame.latch
    void RUnlatchPage(PageId page_id);  // release shared_lock
    void WLatchPage(PageId page_id);    // unique_lock on frame.latch
    void WUnlatchPage(PageId page_id);  // release unique_lock

    // --- Diagnostics ---
    size_t poolSize() const;
    size_t pinnedCount() const;

private:
    frame_id_t findVictim();         // Returns INVALID_FRAME_ID if pool full
    void loadPageFromDisk(frame_id_t frame_id, PageId page_id);
    void flushFrame(frame_id_t frame_id);
};
```

---

## File Structure

```
src/storage/
├── buffer/                        ← new directory
│   ├── CMakeLists.txt
│   ├── frame.h                    ← Frame struct + frame_id_t type
│   ├── lru_replacer.h             ← LruReplacer declaration
│   ├── lru_replacer.cpp           ← LruReplacer implementation
│   ├── buffer_pool.h              ← BufferPool declaration
│   └── buffer_pool.cpp            ← BufferPool implementation
└── (existing files unchanged)

tests/unit/storage/
└── test_buffer_pool.cpp           ← new test file
```

The `src/storage/CMakeLists.txt` is updated to include `buffer/` as a subdirectory.
The `tests/CMakeLists.txt` is updated to include `test_buffer_pool.cpp`.

---

## Concurrency Model

Operations and their locking requirements:

| Operation | Global `latch_` | Frame `latch` |
|-----------|----------------|---------------|
| `FetchPage` (metadata: page_table lookup, replacer) | exclusive | — |
| `FetchPage` (disk read into frame body) | released | write-lock |
| `UnpinPage` | exclusive | — |
| `FlushPage` | shared (to find frame) | write-lock (flush) |
| `RLatchPage` / `WLatchPage` | — | shared / exclusive |

**Key protocol**: The global `latch_` is released before acquiring a frame latch. This prevents a global lock from blocking all page accesses during disk I/O.

---

## Testing Strategy

All tests use TDD: write the test first, then implement to make it pass.

```
test_buffer_pool.cpp

Section: LruReplacer
  - Evict from empty replacer returns false
  - Single frame: unpin then evict succeeds
  - Pin prevents eviction
  - Access promotes from old to young sublist
  - Eviction order: old tail before young frames
  - Midpoint ratio maintained after bulk insertions

Section: BufferPool - single-threaded
  - FetchPage cache miss: loads from disk, returns pinned page
  - FetchPage cache hit: returns same pointer, increments pin_count
  - UnpinPage decrements pin_count; page stays in pool
  - FetchPage on full pool (all pinned) returns nullptr
  - FetchPage on full pool evicts unpinned page
  - Dirty page is flushed before eviction
  - FlushPage writes dirty page without eviction
  - FlushAll flushes all dirty pages
  - Sequential scan does not evict hot pages (midpoint protection)

Section: BufferPool - latching
  - RLatch allows multiple concurrent readers
  - WLatch excludes readers and other writers
  - Latch/unlatch sequence on pinned page succeeds

Section: BufferPool - concurrent (B3-8)
  - Multiple threads fetch/unpin different pages
  - Multiple threads read-latch the same page concurrently
  - Writer thread excludes reader threads via WLatch
```

---

## Dependencies

| Component | Depends On |
|-----------|-----------|
| `frame.h` | `page.h`, `page_id.h` |
| `lru_replacer.h/.cpp` | `frame.h` (for `frame_id_t`) |
| `buffer_pool.h/.cpp` | `frame.h`, `lru_replacer.h`, `page_manager.h`, `common/config.h` |
| `test_buffer_pool.cpp` | `buffer_pool.h`, `page_manager.h` (test double or real) |
