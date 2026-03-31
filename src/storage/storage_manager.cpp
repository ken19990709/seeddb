#include "storage/storage_manager.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#include "storage/page.h"
#include "storage/row_serializer.h"

namespace fs = std::filesystem;

namespace seeddb {

// =============================================================================
// Constructor
// =============================================================================

StorageManager::StorageManager(const std::string& data_dir, const Config& config)
    : data_dir_(data_dir)
    , page_mgr_(data_dir)
    , buffer_pool_(page_mgr_, config)
{
    fs::create_directories(data_dir_);
}

// =============================================================================
// Helpers
// =============================================================================

std::string StorageManager::catalogMetaPath() const {
    return data_dir_ + "/catalog.meta";
}

// ---------------------------------------------------------------------------
// catalog.meta binary format:
//
//   [4 bytes uint32_t: table_count]
//   For each table:
//     [4 bytes uint32_t: name_length]
//     [name_length bytes: table name (UTF-8)]
//     [4 bytes uint32_t: column_count]
//     For each column:
//       [4 bytes uint32_t: col_name_length]
//       [col_name_length bytes: column name]
//       [1 byte uint8_t: LogicalTypeId]
//       [1 byte uint8_t: nullable (0=not nullable, 1=nullable)]
// ---------------------------------------------------------------------------

bool StorageManager::saveCatalogMeta() const {
    const std::string path = catalogMetaPath();
    const std::string temp_path = path + ".tmp";
    FILE* fp = std::fopen(temp_path.c_str(), "wb");
    if (!fp) return false;

    bool write_ok = true;
    auto write_u32 = [fp, &write_ok](uint32_t v) {
        if (write_ok && std::fwrite(&v, sizeof(uint32_t), 1, fp) != 1) {
            write_ok = false;
        }
    };
    auto write_u8 = [fp, &write_ok](uint8_t v) {
        if (write_ok && std::fwrite(&v, sizeof(uint8_t), 1, fp) != 1) {
            write_ok = false;
        }
    };
    auto write_str = [&](const std::string& s) {
        write_u32(static_cast<uint32_t>(s.size()));
        if (write_ok && !s.empty() && std::fwrite(s.data(), 1, s.size(), fp) != s.size()) {
            write_ok = false;
        }
    };

    write_u32(static_cast<uint32_t>(schemas_.size()));

    for (const auto& [name, schema] : schemas_) {
        write_str(name);
        write_u32(static_cast<uint32_t>(schema.columnCount()));
        for (size_t i = 0; i < schema.columnCount(); ++i) {
            const ColumnSchema& col = schema.column(i);
            write_str(col.name());
            write_u8(static_cast<uint8_t>(col.type().id()));
            write_u8(col.isNullable() ? 1u : 0u);
        }
    }

    std::fclose(fp);

    if (!write_ok) {
        // Write failed - remove temp file
        std::remove(temp_path.c_str());
        return false;
    }

    // Atomic rename: temp file -> final file
    if (std::rename(temp_path.c_str(), path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }

    return true;
}

bool StorageManager::loadCatalogMeta() {
    const std::string path = catalogMetaPath();
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return true;  // no catalog yet — empty database is fine

    // Validation constants
    constexpr uint32_t MAX_TABLE_COUNT = 10000;
    constexpr uint32_t MAX_COLUMN_COUNT = 1000;
    constexpr uint32_t MAX_STRING_LENGTH = 65536;
    constexpr uint8_t MAX_LOGICAL_TYPE_ID = 10;  // Adjust based on LogicalTypeId enum

    auto read_u32 = [fp](uint32_t& v) -> bool {
        return std::fread(&v, sizeof(uint32_t), 1, fp) == 1;
    };
    auto read_u8 = [fp](uint8_t& v) -> bool {
        return std::fread(&v, sizeof(uint8_t), 1, fp) == 1;
    };
    auto read_str = [&](std::string& s, uint32_t max_len) -> bool {
        uint32_t len = 0;
        if (!read_u32(len)) return false;
        if (len > max_len) return false;  // Length validation
        s.resize(len);
        if (len > 0 && std::fread(s.data(), 1, len, fp) != len) return false;
        return true;
    };

    uint32_t table_count = 0;
    if (!read_u32(table_count) || table_count > MAX_TABLE_COUNT) {
        std::fclose(fp);
        return false;
    }

    // Snapshot current state for rollback on failure
    auto schemas_backup = schemas_;

    for (uint32_t t = 0; t < table_count; ++t) {
        std::string table_name;
        if (!read_str(table_name, MAX_STRING_LENGTH)) {
            schemas_ = std::move(schemas_backup);
            std::fclose(fp);
            return false;
        }

        uint32_t col_count = 0;
        if (!read_u32(col_count) || col_count > MAX_COLUMN_COUNT) {
            schemas_ = std::move(schemas_backup);
            std::fclose(fp);
            return false;
        }

        std::vector<ColumnSchema> columns;
        columns.reserve(col_count);

        for (uint32_t c = 0; c < col_count; ++c) {
            std::string col_name;
            if (!read_str(col_name, MAX_STRING_LENGTH)) {
                schemas_ = std::move(schemas_backup);
                std::fclose(fp);
                return false;
            }

            uint8_t type_id_raw = 0;
            uint8_t nullable_raw = 0;
            if (!read_u8(type_id_raw) || !read_u8(nullable_raw)) {
                schemas_ = std::move(schemas_backup);
                std::fclose(fp);
                return false;
            }

            // Validate LogicalTypeId range
            if (type_id_raw > MAX_LOGICAL_TYPE_ID) {
                schemas_ = std::move(schemas_backup);
                std::fclose(fp);
                return false;
            }

            LogicalType type(static_cast<LogicalTypeId>(type_id_raw));
            columns.emplace_back(col_name, type, nullable_raw != 0);
        }

        schemas_.emplace(table_name, Schema(std::move(columns)));
    }

    std::fclose(fp);
    return true;
}

// =============================================================================
// Startup
// =============================================================================

bool StorageManager::load(Catalog& catalog) {
    if (!loadCatalogMeta()) return false;

    for (const auto& [name, schema] : schemas_) {
        uint32_t fid = page_mgr_.openTableFile(name);
        if (fid == INVALID_FILE_ID) continue;

        file_ids_[name] = fid;
        catalog.createTable(name, schema);
        // No row loading — rows are read on-demand via createIterator()
    }

    return true;
}

// =============================================================================
// Mutation hooks
// =============================================================================

bool StorageManager::onCreateTable(const std::string& name, const Schema& schema) {
    schemas_[name] = schema;

    // Create the page file (throws if file already exists on disk)
    uint32_t fid;
    try {
        fid = page_mgr_.createTableFile(name);
    } catch (const std::exception&) {
        // File already exists (e.g. leftover from a previous crash) — open it
        fid = page_mgr_.openTableFile(name);
    }

    if (fid == INVALID_FILE_ID) return false;
    file_ids_[name] = fid;

    return saveCatalogMeta();
}

bool StorageManager::onDropTable(const std::string& name) {
    schemas_.erase(name);
    file_ids_.erase(name);
    page_mgr_.dropTableFile(name);
    return saveCatalogMeta();
}

// =============================================================================
// New disk-based API
// =============================================================================

bool StorageManager::insertRowInternal(uint32_t file_id,
                                       const std::vector<char>& serialized,
                                       uint16_t row_size) {
    // Try to fit into the last existing page
    uint32_t num_pages = page_mgr_.pageCount(file_id);
    if (num_pages > 0) {
        PageId last_pid(file_id, num_pages - 1);
        Page* page = buffer_pool_.FetchPage(last_pid);
        if (page && page->freeSpace() >= row_size + Page::SLOT_SIZE) {
            auto slot = page->insertRecord(serialized.data(), row_size);
            if (slot.has_value()) {
                buffer_pool_.UnpinPage(last_pid, true);
                return true;
            }
        }
        if (page) {
            buffer_pool_.UnpinPage(last_pid, false);
        }
    }

    // Allocate a fresh page through BufferPool to maintain cache consistency
    PageId new_pid;
    Page* page = buffer_pool_.NewPage(file_id, &new_pid);
    if (!page) return false;

    auto slot = page->insertRecord(serialized.data(), row_size);
    if (!slot.has_value()) {
        buffer_pool_.UnpinPage(new_pid, false);
        return false;  // row larger than a single page
    }
    buffer_pool_.UnpinPage(new_pid, true);
    return true;
}

bool StorageManager::insertRow(const std::string& table_name,
                               const Row& row,
                               const Schema& schema) {
    auto it = file_ids_.find(table_name);
    if (it == file_ids_.end()) return false;

    auto serialized = RowSerializer::serialize(row, schema);
    // Check for truncation before casting to uint16_t
    if (serialized.size() > UINT16_MAX) {
        return false;  // Row too large for single page storage
    }
    auto row_size = static_cast<uint16_t>(serialized.size());
    return insertRowInternal(it->second, serialized, row_size);
}

bool StorageManager::updateRow(TID tid, const Row& new_row, const Schema& schema) {
    auto serialized = RowSerializer::serialize(new_row, schema);
    // Check for truncation before casting to uint16_t
    if (serialized.size() > UINT16_MAX) {
        return false;  // Row too large for single page storage
    }
    auto row_size = static_cast<uint16_t>(serialized.size());

    PageId pid(tid.file_id, tid.page_num);
    Page* page = buffer_pool_.FetchPage(pid);
    if (!page) return false;

    // Get the old record size to calculate if new data fits after deletion
    auto [old_data, old_size] = page->getRecord(tid.slot_id);
    if (!old_data) {
        // Old record already deleted or invalid
        buffer_pool_.UnpinPage(pid, false);
        return false;
    }

    // Try same-page update: check if new data fits after deleting old slot
    // Need: row_size (new data) + SLOT_SIZE (new slot entry, since insertRecord doesn't reuse)
    // Have: freeSpace + old_size (freed data bytes)
    if (page->freeSpace() + old_size >= row_size + Page::SLOT_SIZE) {
        // Safe to delete and re-insert on same page
        page->deleteRecord(tid.slot_id);
        page->compact();  // Reclaim space from logical deletion
        auto slot = page->insertRecord(serialized.data(), row_size);
        if (slot.has_value()) {
            buffer_pool_.UnpinPage(pid, true);
            return true;
        }
        // Insertion failed unexpectedly - restore is not possible without WAL
        buffer_pool_.UnpinPage(pid, true);
        return false;
    }

    // Doesn't fit on same page — insert on another page first, then delete old.
    // This ensures we don't lose data if insert fails.
    buffer_pool_.UnpinPage(pid, false);

    if (!insertRowInternal(tid.file_id, serialized, row_size)) {
        // Insert failed - old data is still intact
        return false;
    }

    // Insert succeeded — now safe to delete old slot
    page = buffer_pool_.FetchPage(pid);
    if (!page) {
        // Rare failure: new row inserted but can't delete old.
        // Results in duplicate row. True atomicity requires WAL.
        return true;  // Partial success - new row exists
    }
    page->deleteRecord(tid.slot_id);
    buffer_pool_.UnpinPage(pid, true);
    return true;
}

bool StorageManager::deleteRow(TID tid) {
    PageId pid(tid.file_id, tid.page_num);
    Page* page = buffer_pool_.FetchPage(pid);
    if (!page) return false;

    page->deleteRecord(tid.slot_id);
    buffer_pool_.UnpinPage(pid, true);
    return true;
}

std::unique_ptr<TableIterator> StorageManager::createIterator(
        const std::string& table_name) {
    auto fit = file_ids_.find(table_name);
    if (fit == file_ids_.end()) return nullptr;

    auto sit = schemas_.find(table_name);
    if (sit == schemas_.end()) return nullptr;

    uint32_t file_id = fit->second;
    uint32_t total_pages = page_mgr_.pageCount(file_id);

    return std::make_unique<HeapTableIterator>(
        file_id, total_pages, buffer_pool_, sit->second);
}

uint32_t StorageManager::pageCount(const std::string& table_name) const {
    auto it = file_ids_.find(table_name);
    if (it == file_ids_.end()) return 0;
    return page_mgr_.pageCount(it->second);
}

}  // namespace seeddb
