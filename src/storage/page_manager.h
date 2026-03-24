#ifndef SEEDDB_STORAGE_PAGE_MANAGER_H
#define SEEDDB_STORAGE_PAGE_MANAGER_H

#include "storage/disk_manager.h"
#include "storage/page.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace seeddb {

// =============================================================================
// PageManager
// =============================================================================
//
// High-level API for page storage.  Sits on top of DiskManager and:
//   • Maps logical table names to file_ids and on-disk file paths.
//   • Translates getPage / writePage calls into DiskManager page I/O.
//   • Manages a base directory where all table data files are stored.
//
// File naming convention:  <base_dir>/<table_name>.db
//
// file_id assignment:  simple auto-increment (next_file_id_), reset to 0
// when a new PageManager instance is created.  Callers must not assume
// that file_id values are stable across PageManager instances.
// =============================================================================
class PageManager {
public:
    // =========================================================================
    // Constructor / destructor
    // =========================================================================

    /// Constructs a PageManager rooted at @p base_dir.
    /// The directory is created if it does not already exist.
    explicit PageManager(const std::string& base_dir);

    ~PageManager();

    // Non-copyable
    PageManager(const PageManager&)            = delete;
    PageManager& operator=(const PageManager&) = delete;

    // =========================================================================
    // Table file management
    // =========================================================================

    /// Creates a new, empty table file for @p table_name.
    /// @return The file_id assigned to the table.
    /// @throws std::runtime_error if a file for @p table_name already exists.
    uint32_t createTableFile(const std::string& table_name);

    /// Opens an existing table file for @p table_name.
    /// If the table is already open the existing file_id is returned.
    /// @return The file_id, or INVALID_FILE_ID if the file does not exist.
    uint32_t openTableFile(const std::string& table_name);

    /// Closes the file handle (if open) and removes the file from disk.
    /// @return true if the file was removed, false if it did not exist.
    bool dropTableFile(const std::string& table_name);

    /// Returns true if a data file exists on disk for @p table_name.
    bool tableExists(const std::string& table_name) const;

    // =========================================================================
    // Page I/O
    // =========================================================================

    /// Deserializes the page identified by @p page_id into @p page.
    /// @return false if the page_id is out of range or an I/O error occurs.
    bool getPage(PageId page_id, Page& page);

    /// Serializes @p page and writes it to disk at @p page_id's location.
    /// @return false on I/O error.
    bool writePage(PageId page_id, const Page& page);

    // =========================================================================
    // Page allocation
    // =========================================================================

    /// Allocates a new page in the file identified by @p file_id.
    /// @return The new PageId, or INVALID_PAGE_ID if @p file_id is not open.
    PageId allocatePage(uint32_t file_id);

    /// Returns the number of pages currently allocated in the file.
    /// @return Page count, or 0 if @p file_id is not open.
    uint32_t pageCount(uint32_t file_id) const;

private:
    // =========================================================================
    // Helpers
    // =========================================================================

    /// Returns the filesystem path for @p table_name.
    std::string tablePath(const std::string& table_name) const;

    // =========================================================================
    // Data members
    // =========================================================================

    std::string   base_dir_;
    DiskManager   disk_;
    std::unordered_map<std::string, uint32_t> table_to_file_id_;
    uint32_t      next_file_id_{0};
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_PAGE_MANAGER_H
