#ifndef SEEDDB_COMMON_TYPES_H
#define SEEDDB_COMMON_TYPES_H

#include <cstdint>
#include <limits>

namespace seeddb {

// =============================================================================
// Object Identifiers
// =============================================================================

/// Object ID - unique identifier for database objects (tables, indexes, etc.)
/// PostgreSQL equivalent: Oid (4 bytes)
using ObjectId = uint32_t;

/// Transaction ID - unique identifier for transactions
/// PostgreSQL equivalent: TransactionId (4 bytes, we use 8 for safety)
using TransactionId = uint64_t;

/// Log Sequence Number - position in WAL
/// PostgreSQL equivalent: XLogRecPtr (8 bytes)
using Lsn = uint64_t;

/// Page ID - unique identifier for a page (tablespace + file + page number)
/// PostgreSQL equivalent: BlockNumber (4 bytes per component)
struct PageId {
    uint32_t tablespace_id;
    uint32_t file_id;
    uint32_t page_num;

    bool operator==(const PageId& other) const {
        return tablespace_id == other.tablespace_id
            && file_id == other.file_id
            && page_num == other.page_num;
    }

    bool operator!=(const PageId& other) const {
        return !(*this == other);
    }
};

/// Slot ID - index within a slotted page
using SlotId = uint16_t;

// =============================================================================
// Invalid Constants
// =============================================================================

constexpr ObjectId INVALID_OBJECT_ID = std::numeric_limits<ObjectId>::max();
constexpr TransactionId INVALID_TRANSACTION_ID = std::numeric_limits<TransactionId>::max();
constexpr Lsn INVALID_LSN = std::numeric_limits<Lsn>::max();
constexpr PageId INVALID_PAGE_ID = {
    std::numeric_limits<uint32_t>::max(),
    std::numeric_limits<uint32_t>::max(),
    std::numeric_limits<uint32_t>::max()
};

// =============================================================================
// Page Constants
// =============================================================================

/// Default page size (8KB, same as PostgreSQL default)
constexpr size_t DEFAULT_PAGE_SIZE = 8192;

/// Maximum number of columns per table
constexpr size_t MAX_COLUMNS = 1600;

/// Maximum identifier length (table names, column names, etc.)
constexpr size_t MAX_IDENTIFIER_LENGTH = 63;

} // namespace seeddb

#endif // SEEDDB_COMMON_TYPES_H
