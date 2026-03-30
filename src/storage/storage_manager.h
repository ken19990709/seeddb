#ifndef SEEDDB_STORAGE_STORAGE_MANAGER_H
#define SEEDDB_STORAGE_STORAGE_MANAGER_H

#include <memory>
#include <string>
#include <unordered_map>

#include "common/config.h"
#include "storage/buffer/buffer_pool.h"
#include "storage/catalog.h"
#include "storage/page_manager.h"
#include "storage/row.h"
#include "storage/schema.h"
#include "storage/tid.h"
#include "storage/table_iterator.h"

namespace seeddb {

// =============================================================================
// StorageManager — disk-based storage engine
// =============================================================================
//
// Responsibilities:
//  • Persists table schemas to <data_dir>/catalog.meta (binary format).
//  • Persists row data to <data_dir>/<table_name>.db (slotted pages via BufferPool).
//  • On startup, loads schemas back into Catalog. Rows are read on-demand via iterators.
//
// Usage:
//   StorageManager sm(config.data_directory(), config);
//   sm.load(catalog);                          // restore schemas on startup
//   Executor exec(catalog, &sm);               // executor uses new disk API
//
// Persistence strategy:
//  • CREATE TABLE  → save catalog.meta + create .db file
//  • INSERT        → insertRow() via BufferPool
//  • UPDATE        → updateRow() in-place via BufferPool
//  • DELETE        → deleteRow() marks slot deleted via BufferPool
//  • SELECT        → createIterator() scans pages via BufferPool
//  • DROP TABLE    → delete .db file + save catalog.meta
// =============================================================================

class StorageManager {
public:
    // =========================================================================
    // Constructor
    // =========================================================================

    StorageManager(const std::string& data_dir, const Config& config);

    // Non-copyable
    StorageManager(const StorageManager&)            = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // =========================================================================
    // Startup
    // =========================================================================

    /// Loads all persisted table schemas into @p catalog.
    /// Should be called once at startup before any SQL is executed.
    /// @return true on success (including the empty-database case).
    bool load(Catalog& catalog);

    // =========================================================================
    // Mutation hooks (called by Executor after in-memory mutations succeed)
    // =========================================================================

    /// Persists a newly created table: saves catalog.meta and creates the .db file.
    /// @param name   Table name.
    /// @param schema The table schema.
    /// @return true on success.
    bool onCreateTable(const std::string& name, const Schema& schema);

    /// Removes a dropped table from disk and updates catalog.meta.
    /// @param name Table name.
    /// @return true on success.
    bool onDropTable(const std::string& name);

    // =========================================================================
    // New disk-based API
    // =========================================================================

    /// Creates an iterator for scanning all rows of a table via BufferPool.
    std::unique_ptr<TableIterator> createIterator(const std::string& table_name);

    /// Inserts a row into a table via BufferPool.
    bool insertRow(const std::string& table_name, const Row& row, const Schema& schema);

    /// Updates a row in-place (delete old slot + re-insert new data).
    bool updateRow(TID tid, const Row& new_row, const Schema& schema);

    /// Marks a row as deleted.
    bool deleteRow(TID tid);

    /// Returns the number of pages in a table's file.
    uint32_t pageCount(const std::string& table_name) const;

private:
    // =========================================================================
    // Internal helpers
    // =========================================================================

    /// Returns the filesystem path of the catalog metadata file.
    std::string catalogMetaPath() const;

    /// Serializes schemas_ to catalog.meta.
    bool saveCatalogMeta() const;

    /// Deserializes schemas_ from catalog.meta.
    /// Returns true even when the file does not yet exist (empty database).
    bool loadCatalogMeta();

    /// Internal insert that takes file_id directly.
    bool insertRowInternal(uint32_t file_id, const std::vector<char>& serialized, uint16_t row_size);

    // =========================================================================
    // Data members
    // =========================================================================

    std::string   data_dir_;                              ///< Root data directory.
    PageManager   page_mgr_;                              ///< Page I/O subsystem.
    BufferPool    buffer_pool_;                           ///< In-memory page cache.
    std::unordered_map<std::string, Schema>   schemas_;   ///< Cached table schemas.
    std::unordered_map<std::string, uint32_t> file_ids_;  ///< table_name → file_id.
};

}  // namespace seeddb

#endif  // SEEDDB_STORAGE_STORAGE_MANAGER_H
