#ifndef SEEDDB_STORAGE_PAGE_HEADER_H
#define SEEDDB_STORAGE_PAGE_HEADER_H

#include "storage/page_id.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace seeddb {

// =============================================================================
// PageHeader
// =============================================================================

/// Fixed-size header stored at the beginning of every page.
///
/// Layout (little-endian serialization):
///   Offset  Size  Field
///   ------  ----  -----
///    0       4    page_id.file_id
///    4       4    page_id.page_num
///    8       4    free_space_offset  (lower cursor: end of slot array)
///   12       2    slot_count
///   14       1    page_type
///   15       1    (padding)
///   16       4    checksum
///   20       4    upper_offset       (upper cursor: start of record data area)
///   24       8    lsn
///   32       4    prev_page.file_id
///   36       4    prev_page.page_num
///   40       4    next_page.file_id
///   44       4    next_page.page_num
///   48      16    (reserved for future use)
///
/// Conventions (same as PostgreSQL heap):
///   free_space_offset (pd_lower): byte offset of the first free byte after
///     the slot directory. Starts at HEADER_SIZE and grows upward on inserts.
///   upper_offset (pd_upper): byte offset of the first record data byte.
///     Starts at PAGE_SIZE and shrinks downward on inserts.
///   Free space = upper_offset - free_space_offset
struct PageHeader {
    // =========================================================================
    // Constants
    // =========================================================================

    /// Serialized header size in bytes. Chosen to be 64 bytes (cache-line
    /// friendly) and larger than the raw struct to leave room for future fields.
    static constexpr uint32_t HEADER_SIZE = 64;

    // =========================================================================
    // Fields
    // =========================================================================

    PageId   page_id;            ///< Unique page identifier.
    uint32_t free_space_offset;  ///< Lower cursor: byte offset of first free byte after the slot array.
    uint16_t slot_count;         ///< Number of slot entries in the slot array.
    PageType page_type;          ///< Type of this page.
    uint8_t  _pad0{0};           ///< Padding byte.
    uint32_t checksum;           ///< CRC32 checksum of page body (0 = unchecked).
    uint32_t upper_offset;       ///< Upper cursor: byte offset of the first record data byte (grows down).
    uint64_t lsn;                ///< Log Sequence Number for WAL recovery.
    PageId   prev_page;          ///< Previous page in linked list (e.g., B+ tree leaf chain).
    PageId   next_page;          ///< Next page in linked list.

    // =========================================================================
    // Constructor
    // =========================================================================

    /// Default-initializes all fields to zero / invalid.
    PageHeader()
        : page_id()
        , free_space_offset(0)
        , slot_count(0)
        , page_type(PageType::DATA_PAGE)
        , _pad0(0)
        , checksum(0)
        , upper_offset(0)
        , lsn(0)
        , prev_page()
        , next_page()
    {}

    // =========================================================================
    // Serialization
    // =========================================================================

    /// Serializes this header into @p buf.
    /// @param buf Destination buffer of at least HEADER_SIZE bytes.
    void serialize(char* buf) const {
        // Zero the whole reserved area first so padding bytes are deterministic
        std::memset(buf, 0, HEADER_SIZE);

        uint32_t file_id   = page_id.fileId();
        uint32_t page_num  = page_id.pageNum();
        uint32_t fso       = free_space_offset;
        uint16_t sc        = slot_count;
        uint8_t  pt        = static_cast<uint8_t>(page_type);
        uint32_t csum      = checksum;
        uint64_t lsn_val   = lsn;
        uint32_t prev_fid  = prev_page.fileId();
        uint32_t prev_pnum = prev_page.pageNum();
        uint32_t next_fid  = next_page.fileId();
        uint32_t next_pnum = next_page.pageNum();

        char* p = buf;
        std::memcpy(p +  0, &file_id,   4);
        std::memcpy(p +  4, &page_num,  4);
        std::memcpy(p +  8, &fso,       4);
        std::memcpy(p + 12, &sc,        2);
        std::memcpy(p + 14, &pt,        1);
        // byte 15: padding, already zeroed
        std::memcpy(p + 16, &csum,      4);
        std::memcpy(p + 20, &upper_offset, 4);
        std::memcpy(p + 24, &lsn_val,   8);
        std::memcpy(p + 32, &prev_fid,  4);
        std::memcpy(p + 36, &prev_pnum, 4);
        std::memcpy(p + 40, &next_fid,  4);
        std::memcpy(p + 44, &next_pnum, 4);
        // bytes 48-63: reserved, already zeroed
    }

    /// Deserializes this header from @p buf.
    /// @param buf Source buffer of at least HEADER_SIZE bytes.
    void deserialize(const char* buf) {
        uint32_t file_id   = 0;
        uint32_t page_num  = 0;
        uint32_t fso       = 0;
        uint16_t sc        = 0;
        uint8_t  pt        = 0;
        uint32_t csum      = 0;
        uint32_t upper     = 0;
        uint64_t lsn_val   = 0;
        uint32_t prev_fid  = 0;
        uint32_t prev_pnum = 0;
        uint32_t next_fid  = 0;
        uint32_t next_pnum = 0;

        const char* p = buf;
        std::memcpy(&file_id,   p +  0, 4);
        std::memcpy(&page_num,  p +  4, 4);
        std::memcpy(&fso,       p +  8, 4);
        std::memcpy(&sc,        p + 12, 2);
        std::memcpy(&pt,        p + 14, 1);
        std::memcpy(&csum,      p + 16, 4);
        std::memcpy(&upper,     p + 20, 4);
        std::memcpy(&lsn_val,   p + 24, 8);
        std::memcpy(&prev_fid,  p + 32, 4);
        std::memcpy(&prev_pnum, p + 36, 4);
        std::memcpy(&next_fid,  p + 40, 4);
        std::memcpy(&next_pnum, p + 44, 4);

        page_id           = PageId(file_id, page_num);
        free_space_offset = fso;
        slot_count        = sc;
        page_type         = static_cast<PageType>(pt);
        checksum          = csum;
        upper_offset      = upper;
        lsn               = lsn_val;
        prev_page         = PageId(prev_fid, prev_pnum);
        next_page         = PageId(next_fid, next_pnum);
    }
};

static_assert(PageHeader::HEADER_SIZE >= sizeof(PageHeader),
              "HEADER_SIZE must be >= sizeof(PageHeader)");

} // namespace seeddb

#endif // SEEDDB_STORAGE_PAGE_HEADER_H
