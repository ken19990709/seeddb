#ifndef SEEDDB_STORAGE_PAGE_ID_H
#define SEEDDB_STORAGE_PAGE_ID_H

#include <cstdint>
#include <functional>
#include <string>

namespace seeddb {

// =============================================================================
// Page Constants
// =============================================================================

/// Default page size: 4KB (4096 bytes)
constexpr uint32_t PAGE_SIZE = 4096;

/// Invalid file ID marker
constexpr uint32_t INVALID_FILE_ID = UINT32_MAX;

/// Invalid page number marker
constexpr uint32_t INVALID_PAGE_NUM = UINT32_MAX;

// =============================================================================
// PageType Enum
// =============================================================================

/// Enumeration of page types in the storage engine.
enum class PageType : uint8_t {
    DATA_PAGE = 0,      ///< Heap/data page for storing table rows
    INDEX_PAGE = 1,     ///< B+ tree index page
    OVERFLOW_PAGE = 2,  ///< Overflow page for large values
};

// =============================================================================
// PageId Class
// =============================================================================

/// Represents a unique page identifier using a two-part scheme: (file_id, page_num).
/// - file_id: identifies which data file (each table has its own file)
/// - page_num: page number within that file (0-indexed)
class PageId {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor creates an invalid PageId.
    PageId() : file_id_(INVALID_FILE_ID), page_num_(INVALID_PAGE_NUM) {}

    /// Constructs a PageId with the given file_id and page_num.
    /// @param file_id The file identifier.
    /// @param page_num The page number within the file.
    PageId(uint32_t file_id, uint32_t page_num)
        : file_id_(file_id), page_num_(page_num) {}

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the file identifier.
    /// @return The file ID.
    uint32_t fileId() const { return file_id_; }

    /// Returns the page number within the file.
    /// @return The page number.
    uint32_t pageNum() const { return page_num_; }

    /// Checks if this PageId is valid.
    /// @return true if valid, false otherwise.
    bool is_valid() const {
        return file_id_ != INVALID_FILE_ID && page_num_ != INVALID_PAGE_NUM;
    }

    /// Calculates the byte offset of this page within its file.
    /// @return The byte offset (page_num * PAGE_SIZE).
    uint64_t offset() const {
        return static_cast<uint64_t>(page_num_) * PAGE_SIZE;
    }

    // =========================================================================
    // Comparison Operators (defined as friend functions for ADL)
    // =========================================================================

    friend bool operator==(const PageId& lhs, const PageId& rhs) {
        return lhs.file_id_ == rhs.file_id_ && lhs.page_num_ == rhs.page_num_;
    }

    friend bool operator!=(const PageId& lhs, const PageId& rhs) {
        return !(lhs == rhs);
    }

    /// Less-than comparison for ordering (file_id first, then page_num).
    friend bool operator<(const PageId& lhs, const PageId& rhs) {
        if (lhs.file_id_ != rhs.file_id_) {
            return lhs.file_id_ < rhs.file_id_;
        }
        return lhs.page_num_ < rhs.page_num_;
    }

    // =========================================================================
    // Debug
    // =========================================================================

    /// Returns a string representation of this PageId.
    /// @return String in format "(file_id, page_num)" or "(INVALID)".
    std::string toString() const {
        if (!is_valid()) {
            return "(INVALID)";
        }
        return "(" + std::to_string(file_id_) + ", " + std::to_string(page_num_) + ")";
    }

private:
    uint32_t file_id_;   ///< File identifier.
    uint32_t page_num_;  ///< Page number within the file.
};

// =============================================================================
// Constants
// =============================================================================

/// Invalid PageId constant for convenience.
inline const PageId INVALID_PAGE_ID;

} // namespace seeddb

// =============================================================================
// Hash Support for std::unordered_map/set
// =============================================================================

namespace std {

template <>
struct hash<seeddb::PageId> {
    size_t operator()(const seeddb::PageId& page_id) const noexcept {
        // Combine file_id and page_num into a single hash
        // Use a technique similar to boost::hash_combine
        size_t seed = std::hash<uint32_t>{}(page_id.fileId());
        seed ^= std::hash<uint32_t>{}(page_id.pageNum()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

} // namespace std

#endif // SEEDDB_STORAGE_PAGE_ID_H
