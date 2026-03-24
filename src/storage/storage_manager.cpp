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

StorageManager::StorageManager(const std::string& data_dir)
    : data_dir_(data_dir), page_mgr_(data_dir) {
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
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;

    auto write_u32 = [fp](uint32_t v) {
        std::fwrite(&v, sizeof(uint32_t), 1, fp);
    };
    auto write_u8 = [fp](uint8_t v) {
        std::fwrite(&v, sizeof(uint8_t), 1, fp);
    };
    auto write_str = [&](const std::string& s) {
        write_u32(static_cast<uint32_t>(s.size()));
        std::fwrite(s.data(), 1, s.size(), fp);
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
    return true;
}

bool StorageManager::loadCatalogMeta() {
    const std::string path = catalogMetaPath();
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return true;  // no catalog yet — empty database is fine

    auto read_u32 = [fp](uint32_t& v) -> bool {
        return std::fread(&v, sizeof(uint32_t), 1, fp) == 1;
    };
    auto read_u8 = [fp](uint8_t& v) -> bool {
        return std::fread(&v, sizeof(uint8_t), 1, fp) == 1;
    };
    auto read_str = [&](std::string& s) -> bool {
        uint32_t len = 0;
        if (!read_u32(len)) return false;
        s.resize(len);
        return std::fread(s.data(), 1, len, fp) == len;
    };

    uint32_t table_count = 0;
    if (!read_u32(table_count)) { std::fclose(fp); return false; }

    for (uint32_t t = 0; t < table_count; ++t) {
        std::string table_name;
        if (!read_str(table_name)) { std::fclose(fp); return false; }

        uint32_t col_count = 0;
        if (!read_u32(col_count)) { std::fclose(fp); return false; }

        std::vector<ColumnSchema> columns;
        columns.reserve(col_count);

        for (uint32_t c = 0; c < col_count; ++c) {
            std::string col_name;
            if (!read_str(col_name)) { std::fclose(fp); return false; }

            uint8_t type_id_raw = 0;
            uint8_t nullable_raw = 0;
            if (!read_u8(type_id_raw) || !read_u8(nullable_raw)) {
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

std::vector<Row> StorageManager::loadTableRows(const std::string& table_name,
                                                const Schema& schema,
                                                uint32_t file_id) {
    (void)table_name;
    std::vector<Row> rows;
    const uint32_t num_pages = page_mgr_.pageCount(file_id);

    for (uint32_t pn = 0; pn < num_pages; ++pn) {
        PageId pid(file_id, pn);
        Page page;
        if (!page_mgr_.getPage(pid, page)) continue;

        for (uint16_t slot = 0; slot < page.slotCount(); ++slot) {
            auto [data, size] = page.getRecord(slot);
            if (!data || size == 0) continue;  // deleted slot

            rows.push_back(RowSerializer::deserialize(data, size, schema));
        }
    }

    return rows;
}

// =============================================================================
// Startup
// =============================================================================

bool StorageManager::load(Catalog& catalog) {
    if (!loadCatalogMeta()) return false;

    for (const auto& [name, schema] : schemas_) {
        uint32_t fid = page_mgr_.openTableFile(name);
        if (fid == INVALID_FILE_ID) continue;  // data file missing — skip silently

        file_ids_[name] = fid;

        std::vector<Row> rows = loadTableRows(name, schema, fid);

        catalog.createTable(name, schema);
        Table* table = catalog.getTable(name);
        for (auto& row : rows) {
            table->insert(std::move(row));
        }
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

bool StorageManager::appendRow(const std::string& table_name,
                               const Row& row,
                               const Schema& schema) {
    auto it = file_ids_.find(table_name);
    if (it == file_ids_.end()) return false;
    const uint32_t file_id = it->second;

    const auto serialized = RowSerializer::serialize(row, schema);
    const auto row_size = static_cast<uint16_t>(serialized.size());

    // Try to fit into the last existing page
    const uint32_t num_pages = page_mgr_.pageCount(file_id);
    if (num_pages > 0) {
        PageId last_pid(file_id, num_pages - 1);
        Page page;
        if (page_mgr_.getPage(last_pid, page)) {
            if (page.freeSpace() >= row_size + Page::SLOT_SIZE) {
                if (page.insertRecord(serialized.data(), row_size).has_value()) {
                    return page_mgr_.writePage(last_pid, page);
                }
            }
        }
    }

    // Allocate a fresh page
    PageId new_pid = page_mgr_.allocatePage(file_id);
    if (!new_pid.isValid()) return false;

    Page new_page(new_pid, PageType::DATA_PAGE);
    if (!new_page.insertRecord(serialized.data(), row_size).has_value()) {
        return false;  // row larger than a single page — not supported
    }
    return page_mgr_.writePage(new_pid, new_page);
}

bool StorageManager::checkpoint(const std::string& table_name, const Table& table) {
    // Drop and recreate the file so stale pages are removed
    page_mgr_.dropTableFile(table_name);

    uint32_t fid;
    try {
        fid = page_mgr_.createTableFile(table_name);
    } catch (const std::exception&) {
        return false;
    }
    if (fid == INVALID_FILE_ID) return false;
    file_ids_[table_name] = fid;

    const Schema& schema = table.schema();
    for (size_t i = 0; i < table.rowCount(); ++i) {
        if (!appendRow(table_name, table.get(i), schema)) return false;
    }

    return true;
}

}  // namespace seeddb
