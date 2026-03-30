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
#include "storage/table.h"
#include "storage/tid.h"
#include "storage/table_iterator.h"

namespace seeddb {

// =============================================================================
// StorageManager — bridges the SQL engine and the page-storage engine
// =============================================================================
//
// Responsibilities:
//  • Persists table schemas to <data_dir>/catalog.meta (binary format).
//  • Persists row data to <data_dir>/<table_name>.db (slotted pages).
//  • On startup, loads everything back into the provided in-memory Catalog.
//
// Usage in the CLI:
//   StorageManager sm(config.data_directory());
//   sm.load(catalog);                          // restore on startup
//   Executor exec(catalog, &sm);               // executor calls sm after mutations
//
// Persistence strategy:
//  • CREATE TABLE  → save catalog.meta + create .db file
//  • INSERT        → append one row to the last (or a new) slotted page
//  • UPDATE/DELETE → full checkpoint: drop+recreate .db + re-write all rows
//  • DROP TABLE    → delete .db file + save catalog.meta
//
// Backward compatibility:
//   Executor accepts StorageManager* (nullptr = no persistence, existing tests
//   continue to pass without modification).
// =============================================================================

class StorageManager {
public:
    // =========================================================================
    // Constructor
    // =========================================================================

    // New Config-based constructor
    StorageManager(const std::string& data_dir, const Config& config);

    /// Compatibility constructor. Remove when executor switches to new API.
    explicit StorageManager(const std::string& data_dir)
        : StorageManager(data_dir, Config{}) {}

    // Non-copyable
    StorageManager(const StorageManager&)            = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    // =========================================================================
    // Startup
    // =========================================================================

    /// Loads all persisted tables (schemas + rows) into @p catalog.
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

    /// Appends a single row to the table's page file.
    /// Tries to fit in the last allocated page; allocates a new page if full.
    /// @param table_name Table name.
    /// @param row        The row to append.
    /// @param schema     The table schema (used for serialization).
    /// @return true on success.
    bool appendRow(const std::string& table_name,
                   const Row& row,
                   const Schema& schema);

    /// Rewrites all pages for a table from the current in-memory state.
    /// Used after UPDATE or DELETE to keep disk in sync.
    /// @param table_name Table name.
    /// @param table      The in-memory table (all surviving rows).
    /// @return true on success.
    bool checkpoint(const std::string& table_name, const Table& table);

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

    /// Reads all rows from a table's page file into a vector.
    std::vector<Row> loadTableRows(const std::string& table_name,
                                   const Schema& schema,
                                   uint32_t file_id);

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
