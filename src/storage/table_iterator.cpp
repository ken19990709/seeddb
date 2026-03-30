#include "storage/table_iterator.h"

#include "storage/buffer/buffer_pool.h"
#include "storage/page.h"
#include "storage/row_serializer.h"
#include "storage/schema.h"

namespace seeddb {

HeapTableIterator::HeapTableIterator(uint32_t file_id, uint32_t total_pages,
                                     BufferPool& buffer_pool,
                                     const Schema& schema)
    : file_id_(file_id)
    , total_pages_(total_pages)
    , buffer_pool_(buffer_pool)
    , schema_(schema)
    , pinned_page_id_()  // invalid
{}

HeapTableIterator::~HeapTableIterator() {
    if (pinned_page_id_.isValid()) {
        buffer_pool_.UnpinPage(pinned_page_id_, false);
    }
}

bool HeapTableIterator::next() {
    if (exhausted_) return false;
    row_cached_ = false;

    while (current_page_num_ < total_pages_) {
        // Fetch page if not already pinned
        if (!pinned_page_id_.isValid() ||
            current_page_num_ != pinned_page_id_.pageNum()) {
            // Unpin previous page
            if (pinned_page_id_.isValid()) {
                buffer_pool_.UnpinPage(pinned_page_id_, false);
                current_page_ = nullptr;
            }
            pinned_page_id_ = PageId(file_id_, current_page_num_);
            current_page_ = buffer_pool_.FetchPage(pinned_page_id_);
            if (!current_page_) {
                // I/O error — skip this page
                pinned_page_id_ = PageId();
                ++current_page_num_;
                next_slot_ = 0;
                continue;
            }
        }

        // Scan slots on current page
        while (next_slot_ < current_page_->slotCount()) {
            uint16_t slot_id = next_slot_++;
            auto [data, size] = current_page_->getRecord(slot_id);
            if (data && size > 0) {
                // Found a live record
                current_tid_ = TID{file_id_, current_page_num_, slot_id};
                has_current_ = true;
                return true;
            }
            // Deleted slot — skip
        }

        // All slots consumed on this page — advance
        ++current_page_num_;
        next_slot_ = 0;
    }

    // All pages exhausted
    if (pinned_page_id_.isValid()) {
        buffer_pool_.UnpinPage(pinned_page_id_, false);
        current_page_ = nullptr;
        pinned_page_id_ = PageId();
    }
    exhausted_ = true;
    has_current_ = false;
    return false;
}

const Row& HeapTableIterator::currentRow() const {
    if (!row_cached_ && has_current_) {
        auto [data, size] = current_page_->getRecord(current_tid_.slot_id);
        current_row_ = RowSerializer::deserialize(data, size, schema_);
        row_cached_ = true;
    }
    return current_row_;
}

TID HeapTableIterator::currentTID() const {
    return current_tid_;
}

}  // namespace seeddb
