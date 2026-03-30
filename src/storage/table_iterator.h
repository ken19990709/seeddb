#ifndef SEEDDB_STORAGE_TABLE_ITERATOR_H
#define SEEDDB_STORAGE_TABLE_ITERATOR_H

#include <cstdint>
#include <memory>

#include "storage/page_id.h"
#include "storage/row.h"
#include "storage/schema.h"
#include "storage/tid.h"

namespace seeddb {

class BufferPool;
class Page;

// =============================================================================
// TableIterator — Volcano-style row iterator interface
// =============================================================================
class TableIterator {
public:
    virtual ~TableIterator() = default;

    /// Advance to next row. Returns false if exhausted.
    virtual bool next() = 0;

    /// Get current row (deserialized). Valid only after next() returns true.
    virtual const Row& currentRow() const = 0;

    /// Get current row's TID. Valid only after next() returns true.
    virtual TID currentTID() const = 0;
};

// =============================================================================
// HeapTableIterator — scans all slotted pages of a table via BufferPool
// =============================================================================
class HeapTableIterator : public TableIterator {
public:
    /// Constructs an iterator for the given table file.
    /// @param file_id     Table file identifier.
    /// @param total_pages Cached page count (snapshot at construction time).
    /// @param buffer_pool Reference to the BufferPool for page access.
    /// @param schema      Schema for row deserialization (copied for safety).
    HeapTableIterator(uint32_t file_id, uint32_t total_pages,
                      BufferPool& buffer_pool, const Schema& schema);

    ~HeapTableIterator() override;

    // Non-copyable, non-movable
    HeapTableIterator(const HeapTableIterator&) = delete;
    HeapTableIterator& operator=(const HeapTableIterator&) = delete;
    HeapTableIterator(HeapTableIterator&&) = delete;
    HeapTableIterator& operator=(HeapTableIterator&&) = delete;

    bool next() override;
    const Row& currentRow() const override;
    TID currentTID() const override;

private:
    uint32_t file_id_;
    uint32_t total_pages_;
    BufferPool& buffer_pool_;
    Schema schema_;  // Owned copy to prevent dangling reference

    // Current pinned page (INVALID if none pinned)
    PageId pinned_page_id_;
    Page* current_page_ = nullptr;  // valid only while pinned

    // Iteration position
    uint32_t current_page_num_ = 0;
    uint16_t next_slot_ = 0;

    // Cached current row (lazily deserialized)
    mutable Row current_row_;
    mutable bool row_cached_ = false;
    TID current_tid_;
    bool has_current_ = false;
    bool exhausted_ = false;
};

}  // namespace seeddb

#endif  // SEEDDB_STORAGE_TABLE_ITERATOR_H
