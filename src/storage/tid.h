#ifndef SEEDDB_STORAGE_TID_H
#define SEEDDB_STORAGE_TID_H

#include <cstdint>
#include "storage/page_id.h"  // INVALID_FILE_ID

namespace seeddb {

// =============================================================================
// TID — Tuple Identifier (physical row location)
// =============================================================================
// Follows PostgreSQL's CTID pattern: (file_id, page_num, slot_id) uniquely
// identifies a row within a table's heap file.
struct TID {
    uint32_t file_id{INVALID_FILE_ID};   ///< Table file identifier
    uint32_t page_num{INVALID_PAGE_NUM}; ///< Page number within file
    uint16_t slot_id{0};                 ///< Slot index within page

    /// Returns true if this TID points to a valid location.
    bool is_valid() const { return file_id != INVALID_FILE_ID && page_num != INVALID_PAGE_NUM; }
};

}  // namespace seeddb

#endif  // SEEDDB_STORAGE_TID_H
