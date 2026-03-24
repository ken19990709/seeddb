#ifndef SEEDDB_STORAGE_PAGE_H
#define SEEDDB_STORAGE_PAGE_H

#include "storage/page_header.h"

#include <array>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace seeddb {

// =============================================================================
// Page – PostgreSQL-style Slotted Page
// =============================================================================
//
// Memory layout (PAGE_SIZE bytes total):
//
//   [0 .. HEADER_SIZE)            → serialized PageHeader
//   [HEADER_SIZE .. lower_)       → slot array  (grows upward)
//   [lower_ .. upper_ - HEADER_SIZE) → free space
//   [upper_ - HEADER_SIZE .. PAGE_SIZE - HEADER_SIZE) → record data (grows downward)
//
// All offset values in SlotEntry and header_.free_space_offset are
// page-relative (i.e. measured from byte 0 of the page).
//
// A deleted slot has SlotEntry::size == 0.
// =============================================================================

class Page {
public:
    // =========================================================================
    // Constants
    // =========================================================================

    /// Size of a single slot-directory entry (2-byte offset + 2-byte size).
    static constexpr uint16_t SLOT_SIZE = 4;

    // =========================================================================
    // Internal types
    // =========================================================================

    struct SlotEntry {
        uint16_t offset;  ///< Page-relative byte offset of record data.
        uint16_t size;    ///< Size in bytes; 0 means the slot is deleted.
    };

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor – creates an invalid empty page.
    Page() {
        std::memset(body_, 0, sizeof(body_));
        header_.free_space_offset = PageHeader::HEADER_SIZE;  // lower: no slots yet
        header_.upper_offset      = PAGE_SIZE;                // upper: empty data area
    }

    /// Creates a fresh page with the given id and type, ready for records.
    Page(PageId id, PageType type) {
        std::memset(body_, 0, sizeof(body_));
        header_.page_id           = id;
        header_.page_type         = type;
        header_.slot_count        = 0;
        header_.free_space_offset = PageHeader::HEADER_SIZE;  // lower
        header_.upper_offset      = PAGE_SIZE;                // upper
        header_.lsn               = 0;
        header_.checksum          = 0;
        header_.prev_page         = PageId{};
        header_.next_page         = PageId{};
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns a const reference to the in-memory page header.
    const PageHeader& header() const { return header_; }

    /// Returns the number of slot entries (including deleted ones).
    uint16_t slotCount() const { return header_.slot_count; }

    /// Returns the number of bytes of usable free space.
    uint16_t freeSpace() const {
        // free_space = upper_offset - free_space_offset
        uint32_t lo = header_.free_space_offset;
        uint32_t up = header_.upper_offset;
        if (up <= lo) return 0;
        return static_cast<uint16_t>(up - lo);
    }

    // =========================================================================
    // Record Operations
    // =========================================================================

    /// Inserts a record into the page.
    /// @param record_data Pointer to the raw record bytes.
    /// @param size Number of bytes to insert.
    /// @return The slot id on success, std::nullopt if the page is full.
    std::optional<uint16_t> insertRecord(const char* record_data, uint16_t size) {
        // Need space for the record data plus one new slot entry
        if (size == 0) return std::nullopt;
        uint16_t needed = size + SLOT_SIZE;
        if (freeSpace() < needed) return std::nullopt;

        // 1. Write record data at the top of the data area (growing downward)
        //    new_upper is a page-relative offset
        uint32_t new_upper = header_.upper_offset - size;
        // body index = page_offset - HEADER_SIZE
        uint32_t body_data_offset = new_upper - PageHeader::HEADER_SIZE;
        std::memcpy(body_ + body_data_offset, record_data, size);

        // 2. Write slot entry: stored in body at body[slot_id * SLOT_SIZE]
        //    body index of slot = free_space_offset - HEADER_SIZE
        uint16_t slot_id = header_.slot_count;
        uint32_t slot_body_offset = header_.free_space_offset - PageHeader::HEADER_SIZE;
        SlotEntry entry{static_cast<uint16_t>(new_upper), size};
        std::memcpy(body_ + slot_body_offset,     &entry.offset, 2);
        std::memcpy(body_ + slot_body_offset + 2, &entry.size,   2);

        // 3. Update header cursors
        header_.slot_count++;
        header_.free_space_offset += SLOT_SIZE;   // lower grows up
        header_.upper_offset       = new_upper;   // upper grows down

        return slot_id;
    }

    /// Retrieves a record by slot id.
    /// @return {data_ptr, size} on success; {nullptr, 0} if slot is deleted or
    ///         out of range.
    std::pair<const char*, uint16_t> getRecord(uint16_t slot_id) const {
        if (slot_id >= header_.slot_count) return {nullptr, 0};
        SlotEntry entry = readSlot(slot_id);
        if (entry.size == 0) return {nullptr, 0};  // deleted

        uint32_t body_offset = entry.offset - PageHeader::HEADER_SIZE;
        return {body_ + body_offset, entry.size};
    }

    /// Marks a slot as deleted (logical delete; space is reclaimed by compact()).
    /// @return true on success, false if slot_id is out of range.
    bool deleteRecord(uint16_t slot_id) {
        if (slot_id >= header_.slot_count) return false;
        SlotEntry entry = readSlot(slot_id);
        if (entry.size == 0) return false;  // already deleted
        entry.size = 0;
        writeSlot(slot_id, entry);
        return true;
    }

    /// Compacts the page by moving all live records together and reclaiming
    /// fragmented free space.  Slot ids of surviving records are preserved.
    void compact() {
        // Collect all live records
        struct LiveRecord {
            uint16_t slot_id;
            uint16_t old_offset;
            uint16_t size;
        };
        std::vector<LiveRecord> live;
        live.reserve(header_.slot_count);
        for (uint16_t i = 0; i < header_.slot_count; ++i) {
            SlotEntry e = readSlot(i);
            if (e.size > 0) {
                live.push_back({i, e.offset, e.size});
            }
        }

        // Re-pack: snapshot entire body then place records from PAGE_SIZE downward
        char temp_body[PAGE_SIZE - PageHeader::HEADER_SIZE];
        std::memcpy(temp_body, body_, sizeof(temp_body));

        uint32_t new_upper = PAGE_SIZE;
        for (auto& rec : live) {
            new_upper -= rec.size;
            uint32_t src_body  = rec.old_offset - PageHeader::HEADER_SIZE;
            uint32_t dest_body = new_upper      - PageHeader::HEADER_SIZE;
            std::memcpy(body_ + dest_body, temp_body + src_body, rec.size);
            writeSlot(rec.slot_id, SlotEntry{static_cast<uint16_t>(new_upper), rec.size});
        }

        header_.upper_offset = new_upper;
    }

    // =========================================================================
    // Serialization (raw PAGE_SIZE buffer round-trip)
    // =========================================================================

    /// Serializes the full page into a PAGE_SIZE byte buffer.
    void serialize(char* buf) const {
        // Write header
        header_.serialize(buf);
        // Write body
        std::memcpy(buf + PageHeader::HEADER_SIZE, body_, PAGE_SIZE - PageHeader::HEADER_SIZE);
    }

    /// Deserializes the full page from a PAGE_SIZE byte buffer.
    void deserialize(const char* buf) {
        header_.deserialize(buf);
        std::memcpy(body_, buf + PageHeader::HEADER_SIZE, PAGE_SIZE - PageHeader::HEADER_SIZE);
    }

private:
    // =========================================================================
    // Private Helpers
    // =========================================================================

    /// Lower cursor: body-relative offset of the next slot entry = free_space_offset - HEADER_SIZE.
    uint16_t lower() const {
        return static_cast<uint16_t>(header_.free_space_offset - PageHeader::HEADER_SIZE);
    }

    /// Upper cursor: page-relative offset of current top-of-data.
    uint16_t upper() const {
        return static_cast<uint16_t>(header_.upper_offset);
    }

    /// Reads the slot entry at @p slot_id from the body.
    SlotEntry readSlot(uint16_t slot_id) const {
        uint32_t body_offset = slot_id * SLOT_SIZE;
        SlotEntry entry{};
        std::memcpy(&entry.offset, body_ + body_offset,     2);
        std::memcpy(&entry.size,   body_ + body_offset + 2, 2);
        return entry;
    }

    void writeSlot(uint16_t slot_id, SlotEntry entry) {
        uint32_t body_offset = slot_id * SLOT_SIZE;
        std::memcpy(body_ + body_offset,     &entry.offset, 2);
        std::memcpy(body_ + body_offset + 2, &entry.size,   2);
    }

    // =========================================================================
    // Data Members
    // =========================================================================

    PageHeader header_;                          ///< In-memory page header.
    char body_[PAGE_SIZE - PageHeader::HEADER_SIZE]; ///< Raw body bytes (slots + data).
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_PAGE_H
