#ifndef SEEDDB_STORAGE_DISK_MANAGER_H
#define SEEDDB_STORAGE_DISK_MANAGER_H

#include "storage/page_id.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace seeddb {

// =============================================================================
// DiskManager
// =============================================================================
//
// Manages raw page I/O for one or more data files.
//
// Each logical "file" is identified by a uint32_t file_id and mapped to a
// path on disk.  The file is divided into PAGE_SIZE-byte slots; page_num n
// occupies byte offset [n * PAGE_SIZE, (n+1) * PAGE_SIZE).
//
// Allocation strategy:
//   - allocatePage() first reuses entries from an in-memory free list
//     (populated by deallocatePage()).
//   - If the free list is empty the file is extended by one page of zeros.
//
// Note: The free list is not persisted across restarts (educational scope).
// =============================================================================
class DiskManager {
public:
    DiskManager() = default;
    ~DiskManager();

    // Non-copyable / non-movable to prevent double-close of FILE* handles.
    DiskManager(const DiskManager&)            = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    // =========================================================================
    // File lifecycle
    // =========================================================================

    /// Opens (or creates) a file and associates it with @p file_id.
    /// If the file already exists its current page count is derived from
    /// the file size.  If @p file_id is already open the call is a no-op.
    /// @return true on success, false on I/O error.
    bool openFile(uint32_t file_id, const std::string& path);

    /// Closes and removes the file handle for @p file_id.
    void closeFile(uint32_t file_id);

    /// Closes all open file handles.
    void closeAll();

    /// Returns true if a file is currently open for @p file_id.
    bool isOpen(uint32_t file_id) const;

    /// Returns the number of allocated (written) pages in @p file_id.
    /// Returns 0 if @p file_id is not open.
    uint32_t pageCount(uint32_t file_id) const;

    // =========================================================================
    // Page I/O
    // =========================================================================

    /// Reads the page identified by @p page_id into @p buffer (PAGE_SIZE bytes).
    /// @return false if @p page_id is invalid, the file is not open, or
    ///         @p page_id.pageNum() is out of range.
    bool readPage(PageId page_id, char* buffer);

    /// Writes PAGE_SIZE bytes from @p buffer to the location of @p page_id.
    /// @return false if @p page_id is invalid, the file is not open, or
    ///         @p page_id.pageNum() is out of range.
    bool writePage(PageId page_id, const char* buffer);

    // =========================================================================
    // Page allocation
    // =========================================================================

    /// Allocates a new page in @p file_id.
    /// Reuses a page from the in-memory free list when available; otherwise
    /// extends the file by one page filled with zeros.
    /// @return The new PageId, or INVALID_PAGE_ID if @p file_id is not open.
    PageId allocatePage(uint32_t file_id);

    /// Marks @p page_id as free (added to the in-memory free list).
    /// The page data on disk is NOT zeroed.
    void deallocatePage(PageId page_id);

private:
    // =========================================================================
    // Internal types
    // =========================================================================

    struct FileEntry {
        std::string      path;
        FILE*            fp{nullptr};
        uint32_t         num_pages{0};
        std::vector<uint32_t> free_list;
    };

    std::unordered_map<uint32_t, FileEntry> files_;
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_DISK_MANAGER_H
