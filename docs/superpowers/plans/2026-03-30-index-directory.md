# Phase 3.4 Index Directory Extension — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add index metadata infrastructure (IndexSchema, Catalog API, parser, persistence, CLI) to prepare for B+ tree indexes in Phase 3.5.

**Architecture:** Catalog as unified API with separate internal maps for tables and indexes. IndexSchema is a standalone data class. StorageManager handles binary persistence. Parser dispatches CREATE/DROP INDEX via lookahead after CREATE/DROP tokens.

**Tech Stack:** C++17, Catch2, existing SeedDB infrastructure (Result<T>, ExecutionResult, binary catalog.meta)

**Spec:** `docs/superpowers/specs/2026-03-30-index-directory-design.md`

---

## File Structure

| Action | File | Responsibility |
|--------|------|---------------|
| Create | `src/storage/index_schema.h` | IndexType, IndexSortDir, IndexColumn, IndexSchema class |
| Create | `src/storage/index_schema.cpp` | IndexSchema method implementations |
| Modify | `src/common/error.h` | Add DUPLICATE_INDEX=205, REDUNDANT_INDEX=206 |
| Modify | `src/common/error.cpp` | Add name/message for new error codes |
| Modify | `src/storage/catalog.h` | Add index management API (createIndex, dropIndex, etc.) |
| Modify | `src/storage/storage_manager.h` | Add onCreateIndex, onDropIndex; change saveCatalogMeta signature |
| Modify | `src/storage/storage_manager.cpp` | V2 binary format, index load/save |
| Modify | `src/parser/ast.h` | Add NodeType entries, IndexColumnDef, CreateIndexStmt, DropIndexStmt |
| Modify | `src/parser/parser.h` | Add parseCreateIndex, parseDropIndex declarations |
| Modify | `src/parser/parser.cpp` | Refactor parseStatement dispatch, implement index parsers |
| Modify | `src/executor/executor.h` | Add execute() overloads for index statements |
| Modify | `src/executor/executor.cpp` | Implement executeCreateIndex, executeDropIndex, cascade in dropTable |
| Modify | `src/cli/repl.h` | Add listIndexes declaration |
| Modify | `src/cli/repl.cpp` | Add \di command, dispatch for index statements |
| Create | `tests/unit/storage/test_index_catalog.cpp` | All index unit tests |
| Modify | `tests/CMakeLists.txt` | Register new test file |

---

### Task 1: IndexSchema Data Class + Error Codes

**Files:**
- Create: `src/storage/index_schema.h`
- Create: `src/storage/index_schema.cpp`
- Modify: `src/common/error.h:34` (after `DUPLICATE_COLUMN = 204`)
- Modify: `src/common/error.cpp:29,84` (add cases in both switch statements)
- Create: `tests/unit/storage/test_index_catalog.cpp`
- Modify: `tests/CMakeLists.txt:39` (after last test source)

- [ ] **Step 1: Add DUPLICATE_INDEX and REDUNDANT_INDEX to ErrorCode enum**

In `src/common/error.h`, after line 34 (`DUPLICATE_COLUMN = 204,`), add:

```cpp
        DUPLICATE_INDEX = 205,
        REDUNDANT_INDEX = 206,
```

- [ ] **Step 2: Add error_code_name cases**

In `src/common/error.cpp`, after the `DUPLICATE_COLUMN` case (line 29), add:

```cpp
        case ErrorCode::DUPLICATE_INDEX:  return "DUPLICATE_INDEX";
        case ErrorCode::REDUNDANT_INDEX:  return "REDUNDANT_INDEX";
```

- [ ] **Step 3: Add error_code_message cases**

In `src/common/error.cpp`, after the `DUPLICATE_COLUMN` case (line 84), add:

```cpp
        case ErrorCode::DUPLICATE_INDEX:  return "Index already exists";
        case ErrorCode::REDUNDANT_INDEX:  return "An equivalent index already exists";
```

- [ ] **Step 4: Write the IndexSchema header**

Create `src/storage/index_schema.h`:

```cpp
#ifndef SEEDDB_STORAGE_INDEX_SCHEMA_H
#define SEEDDB_STORAGE_INDEX_SCHEMA_H

#include <string>
#include <vector>

namespace seeddb {

/// Index type enumeration
enum class IndexType {
    BTREE,   // Default — Phase 3.5 implements
    HASH,    // Future
};

/// Sort direction for a single column in the index.
enum class IndexSortDir {
    ASC,
    DESC,
};

/// Represents one column's metadata within an index
struct IndexColumn {
    std::string name;
    IndexSortDir direction;

    explicit IndexColumn(std::string n, IndexSortDir d = IndexSortDir::ASC)
        : name(std::move(n)), direction(d) {}
};

/// Index schema — metadata for a single index
class IndexSchema {
public:
    IndexSchema(std::string name, std::string table_name,
                std::vector<IndexColumn> columns,
                bool is_unique = false,
                bool is_primary = false,
                IndexType type = IndexType::BTREE);

    // Accessors
    const std::string& name() const { return name_; }
    const std::string& tableName() const { return table_name_; }
    const std::vector<IndexColumn>& columns() const { return columns_; }
    bool isUnique() const { return is_unique_; }
    bool isPrimary() const { return is_primary_; }
    IndexType indexType() const { return type_; }

    // Utility
    std::string toString() const;

private:
    std::string name_;
    std::string table_name_;
    std::vector<IndexColumn> columns_;
    bool is_unique_;
    bool is_primary_;
    IndexType type_;
};

}  // namespace seeddb

#endif  // SEEDDB_STORAGE_INDEX_SCHEMA_H
```

- [ ] **Step 5: Write the IndexSchema implementation**

Create `src/storage/index_schema.cpp`:

```cpp
#include "storage/index_schema.h"

namespace seeddb {

IndexSchema::IndexSchema(std::string name, std::string table_name,
                         std::vector<IndexColumn> columns,
                         bool is_unique,
                         bool is_primary,
                         IndexType type)
    : name_(std::move(name)),
      table_name_(std::move(table_name)),
      columns_(std::move(columns)),
      is_unique_(is_unique),
      is_primary_(is_primary),
      type_(type) {}

std::string IndexSchema::toString() const {
    std::string result = "INDEX " + name_ + " ON " + table_name_ + " (";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) result += ", ";
        result += columns_[i].name;
        result += (columns_[i].direction == IndexSortDir::ASC) ? " ASC" : " DESC";
    }
    result += ")";
    if (is_unique_) result += " UNIQUE";
    if (is_primary_) result += " PRIMARY";
    return result;
}

}  // namespace seeddb
```

- [ ] **Step 6: Write failing tests for IndexSchema basics**

Create `tests/unit/storage/test_index_catalog.cpp`:

```cpp
#define CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
#include <catch2/catch_all.hpp>

#include "storage/index_schema.h"
#include "storage/catalog.h"
#include "storage/schema.h"
#include "storage/row.h"
#include "common/error.h"
#include "common/value.h"

using namespace seeddb;

// =============================================================================
// IndexSchema Tests
// =============================================================================

TEST_CASE("IndexSchema basic construction", "[index_schema]") {
    IndexSchema idx("idx_users_name", "users",
                    {IndexColumn("name", IndexSortDir::ASC)},
                    false, false, IndexType::BTREE);

    REQUIRE(idx.name() == "idx_users_name");
    REQUIRE(idx.tableName() == "users");
    REQUIRE(idx.columns().size() == 1);
    REQUIRE(idx.columns()[0].name == "name");
    REQUIRE(idx.columns()[0].direction == IndexSortDir::ASC);
    REQUIRE_FALSE(idx.isUnique());
    REQUIRE_FALSE(idx.isPrimary());
    REQUIRE(idx.indexType() == IndexType::BTREE);
}

TEST_CASE("IndexSchema multi-column with mixed directions", "[index_schema]") {
    IndexSchema idx("idx_multi", "orders",
                    {IndexColumn("status", IndexSortDir::ASC),
                     IndexColumn("created_at", IndexSortDir::DESC)},
                    true);

    REQUIRE(idx.columns().size() == 2);
    REQUIRE(idx.columns()[0].direction == IndexSortDir::ASC);
    REQUIRE(idx.columns()[1].direction == IndexSortDir::DESC);
    REQUIRE(idx.isUnique());
}

TEST_CASE("IndexSchema toString", "[index_schema]") {
    IndexSchema idx("idx_name", "users",
                    {IndexColumn("name", IndexSortDir::ASC)},
                    true);
    REQUIRE(idx.toString() == "INDEX idx_name ON users (name ASC) UNIQUE");
}
```

- [ ] **Step 7: Register test file in CMakeLists.txt**

In `tests/CMakeLists.txt`, after line 39 (`unit/storage/test_table_iterator.cpp`), add:

```
        unit/storage/test_index_catalog.cpp
```

- [ ] **Step 8: Build and run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R index_schema`
Expected: 3 PASS

- [ ] **Step 9: Commit**

```bash
git add src/storage/index_schema.h src/storage/index_schema.cpp \
        src/common/error.h src/common/error.cpp \
        tests/unit/storage/test_index_catalog.cpp tests/CMakeLists.txt
git commit -m "feat(storage): add IndexSchema data class and DUPLICATE_INDEX/REDUNDANT_INDEX error codes"
```

---

### Task 2: Catalog Index API

**Files:**
- Modify: `src/storage/catalog.h` (add index management methods)
- Modify: `tests/unit/storage/test_index_catalog.cpp` (add Catalog tests)

- [ ] **Step 1: Write failing tests for Catalog index CRUD**

Append to `tests/unit/storage/test_index_catalog.cpp`:

```cpp
// =============================================================================
// Catalog Index Tests
// =============================================================================

// Helper: create a catalog with a single table "users(id INT, name VARCHAR)"
static Catalog makeCatalogWithUsers() {
    Catalog cat;
    cat.createTable("users", Schema({
        ColumnSchema("id", LogicalType(LogicalTypeId::INTEGER), false),
        ColumnSchema("name", LogicalType(LogicalTypeId::VARCHAR), true),
    }));
    return cat;
}

TEST_CASE("Catalog createIndex basic", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();

    auto result = cat.createIndex(IndexSchema(
        "idx_users_name", "users",
        {IndexColumn("name", IndexSortDir::ASC)}));

    REQUIRE(result.is_ok());
    REQUIRE(cat.hasIndex("idx_users_name"));
    REQUIRE(cat.indexCount() == 1);
}

TEST_CASE("Catalog getIndex returns schema", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx_users_name", "users",
                                 {IndexColumn("name")}));

    const auto* idx = cat.getIndex("idx_users_name");
    REQUIRE(idx != nullptr);
    REQUIRE(idx->tableName() == "users");
    REQUIRE(idx->columns().size() == 1);
}

TEST_CASE("Catalog getIndex nonexistent returns nullptr", "[catalog_index]") {
    Catalog cat;
    REQUIRE(cat.getIndex("nope") == nullptr);
}

TEST_CASE("Catalog createIndex duplicate name fails", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx_name", "users", {IndexColumn("name")}));

    auto result = cat.createIndex(IndexSchema("idx_name", "users",
                                               {IndexColumn("id")}));
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error().code() == ErrorCode::DUPLICATE_INDEX);
}

TEST_CASE("Catalog createIndex table not found fails", "[catalog_index]") {
    Catalog cat;

    auto result = cat.createIndex(IndexSchema("idx", "nonexistent",
                                               {IndexColumn("col")}));
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error().code() == ErrorCode::TABLE_NOT_FOUND);
}

TEST_CASE("Catalog createIndex column not found fails", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();

    auto result = cat.createIndex(IndexSchema("idx", "users",
                                               {IndexColumn("nonexistent")}));
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error().code() == ErrorCode::COLUMN_NOT_FOUND);
}

TEST_CASE("Catalog createIndex redundant index fails", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx1", "users",
                                 {IndexColumn("name", IndexSortDir::ASC)}));

    auto result = cat.createIndex(IndexSchema("idx2", "users",
                                               {IndexColumn("name", IndexSortDir::ASC)}));
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error().code() == ErrorCode::REDUNDANT_INDEX);
}

TEST_CASE("Catalog dropIndex", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx", "users", {IndexColumn("name")}));

    auto result = cat.dropIndex("idx");
    REQUIRE(result.is_ok());
    REQUIRE_FALSE(cat.hasIndex("idx"));
    REQUIRE(cat.indexCount() == 0);
}

TEST_CASE("Catalog dropIndex nonexistent fails", "[catalog_index]") {
    Catalog cat;
    auto result = cat.dropIndex("nope");
    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.error().code() == ErrorCode::INDEX_NOT_FOUND);
}

TEST_CASE("Catalog getTableIndexes", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx1", "users", {IndexColumn("id")}));
    cat.createIndex(IndexSchema("idx2", "users", {IndexColumn("name")}));

    auto indexes = cat.getTableIndexes("users");
    REQUIRE(indexes.size() == 2);
}

TEST_CASE("Catalog listIndexNames", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx1", "users", {IndexColumn("id")}));
    cat.createIndex(IndexSchema("idx2", "users", {IndexColumn("name")}));

    auto names = cat.listIndexNames();
    REQUIRE(names.size() == 2);
}

TEST_CASE("Catalog dropAllIndexesForTable", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx1", "users", {IndexColumn("id")}));
    cat.createIndex(IndexSchema("idx2", "users", {IndexColumn("name")}));

    cat.dropAllIndexesForTable("users");
    REQUIRE(cat.indexCount() == 0);
}

TEST_CASE("Catalog dropTable cascades to indexes", "[catalog_index]") {
    auto cat = makeCatalogWithUsers();
    cat.createIndex(IndexSchema("idx1", "users", {IndexColumn("id")}));
    cat.createIndex(IndexSchema("idx2", "users", {IndexColumn("name")}));

    cat.dropTable("users");
    REQUIRE(cat.indexCount() == 0);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R catalog_index`
Expected: FAIL — `createIndex` not declared in Catalog

- [ ] **Step 3: Add index management API to Catalog**

Add at top of `src/storage/catalog.h`, after existing includes:

```cpp
#include "common/error.h"
#include "storage/index_schema.h"
```

Add new public section to Catalog class (after `tableCount()`, before Iterator Support):

```cpp
    // =========================================================================
    // Index Management
    // =========================================================================

    /// Create a new index with full validation.
    Result<void> createIndex(IndexSchema index) {
        const std::string& idx_name = index.name();

        // 1. Check duplicate name
        if (indexes_.find(idx_name) != indexes_.end()) {
            return Result<void>::err(ErrorCode::DUPLICATE_INDEX,
                "Index '" + idx_name + "' already exists");
        }

        // 2. Check table exists
        const Table* table = getTable(index.tableName());
        if (!table) {
            return Result<void>::err(ErrorCode::TABLE_NOT_FOUND,
                "Table '" + index.tableName() + "' does not exist");
        }

        // 3. Check all columns exist
        const Schema& schema = table->schema();
        for (const auto& col : index.columns()) {
            if (!schema.hasColumn(col.name)) {
                return Result<void>::err(ErrorCode::COLUMN_NOT_FOUND,
                    "Column '" + col.name + "' does not exist in table '" + index.tableName() + "'");
            }
        }

        // 4. Check redundant index (same table + same columns + same directions)
        for (const auto& [name, existing] : indexes_) {
            if (existing.tableName() == index.tableName() &&
                existing.columns().size() == index.columns().size()) {
                bool identical = true;
                for (size_t i = 0; i < index.columns().size(); ++i) {
                    if (existing.columns()[i].name != index.columns()[i].name ||
                        existing.columns()[i].direction != index.columns()[i].direction) {
                        identical = false;
                        break;
                    }
                }
                if (identical) {
                    return Result<void>::err(ErrorCode::REDUNDANT_INDEX,
                        "An equivalent index already exists on table '" + index.tableName() + "'");
                }
            }
        }

        indexes_.emplace(idx_name, std::move(index));
        return Result<void>::ok();
    }

    /// Drop an index by name.
    Result<void> dropIndex(const std::string& name) {
        auto it = indexes_.find(name);
        if (it == indexes_.end()) {
            return Result<void>::err(ErrorCode::INDEX_NOT_FOUND,
                "Index '" + name + "' does not exist");
        }
        indexes_.erase(it);
        return Result<void>::ok();
    }

    /// Get index by name. Returns nullptr if not found.
    const IndexSchema* getIndex(const std::string& name) const {
        auto it = indexes_.find(name);
        return it != indexes_.end() ? &it->second : nullptr;
    }

    /// Check if an index exists.
    bool hasIndex(const std::string& name) const {
        return indexes_.find(name) != indexes_.end();
    }

    /// Get all indexes for a given table.
    std::vector<const IndexSchema*> getTableIndexes(const std::string& table_name) const {
        std::vector<const IndexSchema*> result;
        for (const auto& [name, idx] : indexes_) {
            if (idx.tableName() == table_name) {
                result.push_back(&idx);
            }
        }
        return result;
    }

    /// List all index names in the database.
    std::vector<std::string> listIndexNames() const {
        std::vector<std::string> names;
        names.reserve(indexes_.size());
        for (const auto& [name, idx] : indexes_) {
            names.push_back(name);
        }
        return names;
    }

    /// Drop all indexes associated with a table.
    void dropAllIndexesForTable(const std::string& table_name) {
        for (auto it = indexes_.begin(); it != indexes_.end(); ) {
            if (it->second.tableName() == table_name) {
                it = indexes_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /// Returns the number of indexes.
    size_t indexCount() const { return indexes_.size(); }
```

Add new private member after `tables_`:

```cpp
    std::unordered_map<std::string, IndexSchema> indexes_;
```

Modify existing `dropTable()` to cascade. Replace the `dropTable` method body:

```cpp
    bool dropTable(const std::string& name) {
        auto it = tables_.find(name);
        if (it == tables_.end()) {
            return false;
        }
        dropAllIndexesForTable(name);
        tables_.erase(it);
        return true;
    }
```

- [ ] **Step 4: Build and run Catalog tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R catalog_index`
Expected: All PASS

- [ ] **Step 5: Commit**

```bash
git add src/storage/catalog.h tests/unit/storage/test_index_catalog.cpp
git commit -m "feat(storage): add Catalog index management API with validation"
```

---

### Task 3: Persistence — V2 Binary catalog.meta Format

**Files:**
- Modify: `src/storage/storage_manager.h` (add hooks, change saveCatalogMeta signature)
- Modify: `src/storage/storage_manager.cpp` (V2 format, index load/save)
- Modify: `tests/unit/storage/test_index_catalog.cpp` (add persistence tests)

- [ ] **Step 1: Write failing persistence tests**

Append to `tests/unit/storage/test_index_catalog.cpp`:

```cpp
#include <filesystem>
#include "storage/storage_manager.h"
#include "common/config.h"

namespace fs = std::filesystem;

// =============================================================================
// Persistence Tests
// =============================================================================

struct TempDirForIndex {
    std::string path;
    seeddb::Config config;

    TempDirForIndex() {
        path = fs::temp_directory_path().string() + "/seeddb_idx_test_" +
               std::to_string(reinterpret_cast<uintptr_t>(this));
        fs::create_directories(path);
        config.set("buffer_pool_size", "10");
    }
    ~TempDirForIndex() { fs::remove_all(path); }
};

TEST_CASE("Persist and reload index", "[index_persistence]") {
    TempDirForIndex tmp;

    // Phase 1: create table + index, persist
    {
        Catalog cat;
        StorageManager sm(tmp.path, tmp.config);
        sm.load(cat);

        cat.createTable("users", Schema({
            ColumnSchema("id", LogicalType(LogicalTypeId::INTEGER), false),
            ColumnSchema("name", LogicalType(LogicalTypeId::VARCHAR), true),
        }));
        sm.onCreateTable("users", cat.getTable("users")->schema());

        IndexSchema idx("idx_name", "users",
                        {IndexColumn("name", IndexSortDir::ASC)}, true);
        cat.createIndex(idx);
        sm.onCreateIndex(cat, idx);
    }

    // Phase 2: reload, verify index survives
    {
        Catalog cat;
        StorageManager sm(tmp.path, tmp.config);
        sm.load(cat);

        REQUIRE(cat.hasIndex("idx_name"));
        const auto* idx = cat.getIndex("idx_name");
        REQUIRE(idx != nullptr);
        REQUIRE(idx->tableName() == "users");
        REQUIRE(idx->columns().size() == 1);
        REQUIRE(idx->columns()[0].name == "name");
        REQUIRE(idx->isUnique());
    }
}

TEST_CASE("Drop index persists", "[index_persistence]") {
    TempDirForIndex tmp;

    {
        Catalog cat;
        StorageManager sm(tmp.path, tmp.config);
        sm.load(cat);

        cat.createTable("t", Schema({
            ColumnSchema("a", LogicalType(LogicalTypeId::INTEGER)),
        }));
        sm.onCreateTable("t", cat.getTable("t")->schema());

        IndexSchema idx("idx_a", "t", {IndexColumn("a")});
        cat.createIndex(idx);
        sm.onCreateIndex(cat, idx);

        cat.dropIndex("idx_a");
        sm.onDropIndex(cat, "idx_a");
    }

    {
        Catalog cat;
        StorageManager sm(tmp.path, tmp.config);
        sm.load(cat);
        REQUIRE_FALSE(cat.hasIndex("idx_a"));
    }
}

TEST_CASE("V1 format loads with empty indexes", "[index_persistence]") {
    TempDirForIndex tmp;

    // Write V1 format: just a table_count=0 file
    {
        std::string path = tmp.path + "/catalog.meta";
        FILE* fp = std::fopen(path.c_str(), "wb");
        uint32_t zero = 0;
        std::fwrite(&zero, sizeof(uint32_t), 1, fp);
        std::fclose(fp);
    }

    Catalog cat;
    StorageManager sm(tmp.path, tmp.config);
    REQUIRE(sm.load(cat));
    REQUIRE(cat.indexCount() == 0);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R index_persistence`
Expected: FAIL — `onCreateIndex` not declared in StorageManager

- [ ] **Step 3: Modify StorageManager header**

In `src/storage/storage_manager.h`:

1. Add include after existing includes:
```cpp
#include "storage/index_schema.h"
```

2. Add forward declaration for Catalog (already has it implicitly through `Catalog&` param, but add explicit):
```cpp
class Catalog;
```
Note: Catalog is already included via `storage/catalog.h`.

3. Add new public methods after `onDropTable`:
```cpp
    /// Persist a newly created index.
    bool onCreateIndex(const Catalog& catalog, const IndexSchema& index);

    /// Persist index deletion.
    bool onDropIndex(const Catalog& catalog, const std::string& name);
```

4. Change `saveCatalogMeta()` signature:
```cpp
    /// Serializes schemas and indexes to catalog.meta.
    bool saveCatalogMeta(const Catalog& catalog) const;
```
Remove the old `const` version.

- [ ] **Step 4: Implement V2 format in StorageManager**

Replace `saveCatalogMeta()` in `src/storage/storage_manager.cpp` with V2 version that accepts Catalog&. Update the binary format comment too:

```cpp
// ---------------------------------------------------------------------------
// catalog.meta V2 binary format:
//
//   [4 bytes uint32_t: version = 2]
//   [4 bytes uint32_t: table_count]
//   For each table:                        // (same as V1)
//     [4 bytes uint32_t: name_length]
//     [name_length bytes: table name (UTF-8)]
//     [4 bytes uint32_t: column_count]
//     For each column:
//       [4 bytes uint32_t: col_name_length]
//       [col_name_length bytes: column name]
//       [1 byte uint8_t: LogicalTypeId]
//       [1 byte uint8_t: nullable (0=not nullable, 1=nullable)]
//   [4 bytes uint32_t: index_count]
//   For each index:
//     [4 bytes uint32_t: name_length]
//     [name_length bytes: index name]
//     [4 bytes uint32_t: table_name_length]
//     [table_name_length bytes: table name]
//     [4 bytes uint32_t: column_count]
//     For each column:
//       [4 bytes uint32_t: col_name_length]
//       [col_name_length bytes: column name]
//       [1 byte uint8_t: IndexSortDir (0=ASC, 1=DESC)]
//     [1 byte uint8_t: is_unique (0/1)]
//     [1 byte uint8_t: is_primary (0/1)]
//     [1 byte uint8_t: IndexType (0=BTREE, 1=HASH)]
// ---------------------------------------------------------------------------

bool StorageManager::saveCatalogMeta(const Catalog& catalog) const {
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

    // V2 header
    write_u32(2);  // version

    // Tables section (same as V1, but now inside V2 wrapper)
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

    // Indexes section (NEW)
    auto index_names = catalog.listIndexNames();
    write_u32(static_cast<uint32_t>(index_names.size()));
    for (const auto& idx_name : index_names) {
        const IndexSchema* idx = catalog.getIndex(idx_name);
        write_str(idx->name());
        write_str(idx->tableName());
        write_u32(static_cast<uint32_t>(idx->columns().size()));
        for (const auto& col : idx->columns()) {
            write_str(col.name);
            write_u8(static_cast<uint8_t>(col.direction));
        }
        write_u8(idx->isUnique() ? 1u : 0u);
        write_u8(idx->isPrimary() ? 1u : 0u);
        write_u8(static_cast<uint8_t>(idx->indexType()));
    }

    std::fclose(fp);
    return true;
}
```

Update `loadCatalogMeta()` to handle V1/V2 detection:

```cpp
bool StorageManager::loadCatalogMeta() {
    const std::string path = catalogMetaPath();
    FILE* fp = std::fopen(path.c_str(), "wb");
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

    // Peek at first 4 bytes to detect V1 vs V2
    uint32_t first_word = 0;
    if (!read_u32(first_word)) { std::fclose(fp); return false; }

    bool is_v2 = (first_word == 2);
    uint32_t table_count = 0;

    if (is_v2) {
        // V2: first word was version=2, next is table_count
        if (!read_u32(table_count)) { std::fclose(fp); return false; }
    } else {
        // V1: first word IS table_count (no version header)
        table_count = first_word;
    }

    // Read tables
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

    // Read indexes (V2 only)
    loaded_indexes_.clear();
    if (is_v2) {
        uint32_t index_count = 0;
        if (!read_u32(index_count)) { std::fclose(fp); return true; }  // no indexes is ok

        for (uint32_t i = 0; i < index_count; ++i) {
            std::string idx_name, tbl_name;
            if (!read_str(idx_name) || !read_str(tbl_name)) { std::fclose(fp); return false; }

            uint32_t col_count = 0;
            if (!read_u32(col_count)) { std::fclose(fp); return false; }

            std::vector<IndexColumn> columns;
            columns.reserve(col_count);
            for (uint32_t c = 0; c < col_count; ++c) {
                std::string col_name;
                if (!read_str(col_name)) { std::fclose(fp); return false; }
                uint8_t dir_raw = 0;
                if (!read_u8(dir_raw)) { std::fclose(fp); return false; }
                columns.emplace_back(std::move(col_name),
                                     static_cast<IndexSortDir>(dir_raw));
            }

            uint8_t unique_raw = 0, primary_raw = 0, type_raw = 0;
            if (!read_u8(unique_raw) || !read_u8(primary_raw) || !read_u8(type_raw)) {
                std::fclose(fp);
                return false;
            }

            loaded_indexes_.emplace_back(std::move(idx_name), std::move(tbl_name),
                std::move(columns), unique_raw != 0, primary_raw != 0,
                static_cast<IndexType>(type_raw));
        }
    }

    std::fclose(fp);
    return true;
}
```

Add `onCreateIndex` and `onDropIndex` implementations:

```cpp
bool StorageManager::onCreateIndex(const Catalog& catalog, const IndexSchema& index) {
    return saveCatalogMeta(catalog);
}

bool StorageManager::onDropIndex(const Catalog& catalog, const std::string& name) {
    return saveCatalogMeta(catalog);
}
```

Update `load()` to transfer indexes to catalog:

```cpp
bool StorageManager::load(Catalog& catalog) {
    if (!loadCatalogMeta()) return false;

    for (const auto& [name, schema] : schemas_) {
        uint32_t fid = page_mgr_.openTableFile(name);
        if (fid == INVALID_FILE_ID) continue;

        file_ids_[name] = fid;
        catalog.createTable(name, schema);
    }

    // Transfer loaded indexes to catalog
    for (auto& idx : loaded_indexes_) {
        catalog.createIndex(std::move(idx));
    }
    loaded_indexes_.clear();

    return true;
}
```

Update all existing call sites of `saveCatalogMeta()` to pass `catalog`:

In `onCreateTable()` and `onDropTable()` — these need a `const Catalog&` parameter now. Update signatures in the header:

```cpp
    bool onCreateTable(const std::string& name, const Schema& schema, const Catalog& catalog);
    bool onDropTable(const std::string& name, const Catalog& catalog);
```

Update implementations to pass catalog:

```cpp
bool StorageManager::onCreateTable(const std::string& name, const Schema& schema, const Catalog& catalog) {
    schemas_[name] = schema;
    uint32_t fid;
    try {
        fid = page_mgr_.createTableFile(name);
    } catch (const std::exception&) {
        fid = page_mgr_.openTableFile(name);
    }
    if (fid == INVALID_FILE_ID) return false;
    file_ids_[name] = fid;
    return saveCatalogMeta(catalog);
}

bool StorageManager::onDropTable(const std::string& name, const Catalog& catalog) {
    schemas_.erase(name);
    file_ids_.erase(name);
    page_mgr_.dropTableFile(name);
    return saveCatalogMeta(catalog);
}
```

Add `loaded_indexes_` private member to StorageManager header:

```cpp
    std::vector<IndexSchema> loaded_indexes_;  ///< Staging area for indexes during load
```

- [ ] **Step 5: Update executor to pass catalog to StorageManager hooks**

In `src/executor/executor.cpp`, update `execute(CreateTableStmt&)` (around line 62):

```cpp
    if (storage_mgr_) {
        storage_mgr_->onCreateTable(table_name, catalog_.getTable(table_name)->schema(), catalog_);
    }
```

Update `execute(DropTableStmt&)` (around line 84-86):

```cpp
    if (storage_mgr_) {
        storage_mgr_->onDropTable(table_name, catalog_);
    }
```

- [ ] **Step 6: Build and run persistence tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R index_persistence`
Expected: All PASS

- [ ] **Step 7: Run ALL existing tests to verify no regression**

Run: `cd build && ctest --output-on-failure`
Expected: All PASS

- [ ] **Step 8: Commit**

```bash
git add src/storage/storage_manager.h src/storage/storage_manager.cpp \
        src/executor/executor.cpp tests/unit/storage/test_index_catalog.cpp
git commit -m "feat(storage): add V2 binary catalog.meta format with index persistence"
```

---

### Task 4: AST Nodes for CREATE/DROP INDEX

**Files:**
- Modify: `src/parser/ast.h` (add NodeType entries, IndexColumnDef, CreateIndexStmt, DropIndexStmt)
- Modify: `src/parser/ast.cpp` (add toString implementations)

- [ ] **Step 1: Add NodeType entries**

In `src/parser/ast.h`, in the `NodeType` enum (after `STMT_DELETE` around line 20), add:

```cpp
    STMT_CREATE_INDEX,
    STMT_DROP_INDEX,
```

- [ ] **Step 2: Add IndexColumnDef struct and statement classes**

In `src/parser/ast.h`, after the `DropTableStmt` class (around line 574), add:

```cpp
/// Index column definition at AST level
struct IndexColumnDef {
    std::string name;
    SortDirection direction;

    IndexColumnDef(std::string n, SortDirection d = SortDirection::ASC)
        : name(std::move(n)), direction(d) {}
};

/// CREATE INDEX statement
class CreateIndexStmt : public Stmt {
public:
    CreateIndexStmt(std::string index_name, std::string table_name,
                    std::vector<IndexColumnDef> columns, bool is_unique)
        : index_name_(std::move(index_name)),
          table_name_(std::move(table_name)),
          columns_(std::move(columns)),
          is_unique_(is_unique) {}

    NodeType type() const override { return NodeType::STMT_CREATE_INDEX; }
    std::string toString() const override;

    const std::string& indexName() const { return index_name_; }
    const std::string& tableName() const { return table_name_; }
    const std::vector<IndexColumnDef>& columns() const { return columns_; }
    bool isUnique() const { return is_unique_; }

private:
    std::string index_name_;
    std::string table_name_;
    std::vector<IndexColumnDef> columns_;
    bool is_unique_;
};

/// DROP INDEX statement
class DropIndexStmt : public Stmt {
public:
    DropIndexStmt(std::string index_name, bool if_exists)
        : index_name_(std::move(index_name)), if_exists_(if_exists) {}

    NodeType type() const override { return NodeType::STMT_DROP_INDEX; }
    std::string toString() const override;

    const std::string& indexName() const { return index_name_; }
    bool hasIfExists() const { return if_exists_; }

private:
    std::string index_name_;
    bool if_exists_;
};
```

- [ ] **Step 3: Add toString implementations**

Find where other `toString()` methods are implemented (likely `src/parser/ast.cpp`). Add:

```cpp
std::string CreateIndexStmt::toString() const {
    std::string result = "CREATE ";
    if (is_unique_) result += "UNIQUE ";
    result += "INDEX " + index_name_ + " ON " + table_name_ + " (";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) result += ", ";
        result += columns_[i].name;
        result += (columns_[i].direction == SortDirection::ASC) ? " ASC" : " DESC";
    }
    result += ")";
    return result;
}

std::string DropIndexStmt::toString() const {
    std::string result = "DROP INDEX " + index_name_;
    if (if_exists_) result += " IF EXISTS";
    return result;
}
```

- [ ] **Step 4: Build to verify compilation**

Run: `cd build && cmake .. && make -j$(nproc)`
Expected: Build succeeds (no parser integration yet, just AST nodes)

- [ ] **Step 5: Commit**

```bash
git add src/parser/ast.h src/parser/ast.cpp
git commit -m "feat(parser): add AST nodes for CREATE INDEX and DROP INDEX statements"
```

---

### Task 5: Parser — Dispatch Refactor + Index Parsing

**Files:**
- Modify: `src/parser/parser.h` (add parseCreateIndex, parseDropIndex declarations)
- Modify: `src/parser/parser.cpp` (refactor parseStatement, implement index parsers)
- Modify: `tests/unit/storage/test_index_catalog.cpp` (add parser tests)

- [ ] **Step 1: Write failing parser tests**

Append to `tests/unit/storage/test_index_catalog.cpp` (add includes at top if not present):

```cpp
#include "parser/lexer.h"
#include "parser/parser.h"

using namespace seeddb::parser;

// =============================================================================
// Parser Index Tests
// =============================================================================

TEST_CASE("Parse CREATE INDEX basic", "[index_parser]") {
    Lexer lexer("CREATE INDEX idx_name ON users (name)");
    Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());

    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.value().get());
    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->indexName() == "idx_name");
    REQUIRE(stmt->tableName() == "users");
    REQUIRE(stmt->columns().size() == 1);
    REQUIRE(stmt->columns()[0].name == "name");
    REQUIRE_FALSE(stmt->isUnique());
}

TEST_CASE("Parse CREATE UNIQUE INDEX", "[index_parser]") {
    Lexer lexer("CREATE UNIQUE INDEX idx ON t (col)");
    Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());

    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.value().get());
    REQUIRE(stmt->isUnique());
}

TEST_CASE("Parse CREATE INDEX multi-column ASC/DESC", "[index_parser]") {
    Lexer lexer("CREATE INDEX idx ON t (a ASC, b DESC)");
    Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());

    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.value().get());
    REQUIRE(stmt->columns().size() == 2);
    REQUIRE(stmt->columns()[0].direction == SortDirection::ASC);
    REQUIRE(stmt->columns()[1].direction == SortDirection::DESC);
}

TEST_CASE("Parse DROP INDEX", "[index_parser]") {
    Lexer lexer("DROP INDEX idx_name");
    Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());

    auto* stmt = dynamic_cast<DropIndexStmt*>(result.value().get());
    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->indexName() == "idx_name");
    REQUIRE_FALSE(stmt->hasIfExists());
}

TEST_CASE("Parse DROP INDEX IF EXISTS", "[index_parser]") {
    Lexer lexer("DROP INDEX idx_name IF EXISTS");
    Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());

    auto* stmt = dynamic_cast<DropIndexStmt*>(result.value().get());
    REQUIRE(stmt->hasIfExists());
}

TEST_CASE("Parse existing statements still work after dispatch refactor", "[index_parser]") {
    SECTION("CREATE TABLE") {
        Lexer lexer("CREATE TABLE t (id INT)");
        Parser parser(lexer);
        auto result = parser.parse();
        REQUIRE(result.is_ok());
        REQUIRE(dynamic_cast<CreateTableStmt*>(result.value().get()) != nullptr);
    }
    SECTION("DROP TABLE") {
        Lexer lexer("DROP TABLE t");
        Parser parser(lexer);
        auto result = parser.parse();
        REQUIRE(result.is_ok());
        REQUIRE(dynamic_cast<DropTableStmt*>(result.value().get()) != nullptr);
    }
    SECTION("DROP TABLE IF EXISTS") {
        Lexer lexer("DROP TABLE IF EXISTS t");
        Parser parser(lexer);
        auto result = parser.parse();
        REQUIRE(result.is_ok());
        auto* stmt = dynamic_cast<DropTableStmt*>(result.value().get());
        REQUIRE(stmt->hasIfExists());
    }
}
```

- [ ] **Step 2: Add parser method declarations**

In `src/parser/parser.h`, after `parseDropTable()` declaration (line 56), add:

```cpp
    Result<std::unique_ptr<CreateIndexStmt>> parseCreateIndex(bool is_unique);
    Result<std::unique_ptr<DropIndexStmt>> parseDropIndex();
```

- [ ] **Step 3: Refactor parseStatement dispatch**

Replace `parseStatement()` in `src/parser/parser.cpp` (lines 67-85) with:

```cpp
Result<std::unique_ptr<Stmt>> Parser::parseStatement() {
    switch (current_token_.type) {
        case TokenType::CREATE: {
            // Consume CREATE
            consume();

            // Check for optional UNIQUE keyword
            bool is_unique = false;
            if (check(TokenType::UNIQUE)) {
                consume();
                is_unique = true;
            }

            // Dispatch: INDEX vs TABLE
            if (check(TokenType::INDEX)) {
                return wrapStatement(parseCreateIndex(is_unique));
            } else if (check(TokenType::TABLE)) {
                return wrapStatement(parseCreateTable());
            } else {
                return syntax_error<std::unique_ptr<Stmt>>(
                    "Expected TABLE or INDEX after CREATE");
            }
        }
        case TokenType::DROP: {
            // Consume DROP
            consume();

            // Dispatch: INDEX vs TABLE
            if (check(TokenType::INDEX)) {
                return wrapStatement(parseDropIndex());
            } else if (check(TokenType::TABLE)) {
                return wrapStatement(parseDropTable());
            } else {
                return syntax_error<std::unique_ptr<Stmt>>(
                    "Expected TABLE or INDEX after DROP");
            }
        }
        case TokenType::SELECT:
            return wrapStatement(parseSelect());
        case TokenType::INSERT:
            return wrapStatement(parseInsert());
        case TokenType::UPDATE:
            return wrapStatement(parseUpdate());
        case TokenType::DELETE:
            return wrapStatement(parseDelete());
        default:
            return syntax_error<std::unique_ptr<Stmt>>(
                "Unexpected token: " + std::string(token_type_name(current_token_.type)));
    }
}
```

- [ ] **Step 4: Update parseCreateTable and parseDropTable to not consume CREATE/DROP**

In `parseCreateTable()`, remove the CREATE consumption (lines 89-91). The method now expects to be called after CREATE TABLE is already consumed:

```cpp
Result<std::unique_ptr<CreateTableStmt>> Parser::parseCreateTable() {
    // CREATE already consumed by parseStatement()
    // Expect TABLE (still needs to be consumed since parseStatement only consumed CREATE)
    if (!match(TokenType::TABLE)) {
        return syntax_error<std::unique_ptr<CreateTableStmt>>("Expected TABLE");
    }

    // ... rest unchanged ...
```

In `parseDropTable()`, similarly remove the DROP consumption:

```cpp
Result<std::unique_ptr<DropTableStmt>> Parser::parseDropTable() {
    // DROP already consumed by parseStatement()
    // Expect TABLE
    if (!match(TokenType::TABLE)) {
        return syntax_error<std::unique_ptr<DropTableStmt>>("Expected TABLE");
    }

    // ... rest unchanged ...
```

- [ ] **Step 5: Implement parseCreateIndex**

Add after `parseDropTable()` implementation:

```cpp
Result<std::unique_ptr<CreateIndexStmt>> Parser::parseCreateIndex(bool is_unique) {
    // Expect INDEX (CREATE [UNIQUE] already consumed)
    if (!match(TokenType::INDEX)) {
        return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected INDEX");
    }

    // Get index name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected index name");
    }
    std::string index_name = std::get<std::string>(current_token_.value);
    consume();

    // Expect ON
    if (!match(TokenType::ON)) {
        return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected ON");
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected table name");
    }
    std::string table_name = std::get<std::string>(current_token_.value);
    consume();

    // Expect (
    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected '('");
    }

    // Parse column list with optional ASC/DESC
    std::vector<IndexColumnDef> columns;
    do {
        if (!check(TokenType::IDENTIFIER)) {
            return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected column name");
        }
        std::string col_name = std::get<std::string>(current_token_.value);
        consume();

        SortDirection dir = SortDirection::ASC;
        if (check(TokenType::ASC)) {
            consume();
        } else if (check(TokenType::DESC)) {
            consume();
            dir = SortDirection::DESC;
        }

        columns.emplace_back(std::move(col_name), dir);
    } while (match(TokenType::COMMA));

    // Expect )
    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<CreateIndexStmt>>("Expected ')'");
    }

    return Result<std::unique_ptr<CreateIndexStmt>>::ok(
        std::make_unique<CreateIndexStmt>(
            std::move(index_name), std::move(table_name),
            std::move(columns), is_unique));
}
```

- [ ] **Step 6: Implement parseDropIndex**

```cpp
Result<std::unique_ptr<DropIndexStmt>> Parser::parseDropIndex() {
    // Expect INDEX (DROP already consumed)
    if (!match(TokenType::INDEX)) {
        return syntax_error<std::unique_ptr<DropIndexStmt>>("Expected INDEX");
    }

    // Optional IF EXISTS
    bool if_exists = false;
    if (match(TokenType::IF)) {
        if (!match(TokenType::EXISTS)) {
            return syntax_error<std::unique_ptr<DropIndexStmt>>("Expected EXISTS after IF");
        }
        if_exists = true;
    }

    // Get index name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<DropIndexStmt>>("Expected index name");
    }
    std::string index_name = std::get<std::string>(current_token_.value);
    consume();

    return Result<std::unique_ptr<DropIndexStmt>>::ok(
        std::make_unique<DropIndexStmt>(std::move(index_name), if_exists));
}
```

- [ ] **Step 7: Verify UNIQUE is a keyword**

Check `src/parser/token.h` — verify `UNIQUE` is in the `TokenType` enum. If not, add it.

- [ ] **Step 8: Build and run parser tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure -R index_parser`
Expected: All PASS

- [ ] **Step 9: Run ALL existing tests to verify parser refactor didn't break anything**

Run: `cd build && ctest --output-on-failure`
Expected: All PASS

- [ ] **Step 10: Commit**

```bash
git add src/parser/parser.h src/parser/parser.cpp \
        tests/unit/storage/test_index_catalog.cpp
git commit -m "feat(parser): add CREATE INDEX and DROP INDEX parsing with dispatch refactor"
```

---

### Task 6: Executor — Index Statement Execution

**Files:**
- Modify: `src/executor/executor.h` (add execute overloads)
- Modify: `src/executor/executor.cpp` (implement executeCreateIndex, executeDropIndex, cascade)

- [ ] **Step 1: Add execute() overloads to executor header**

In `src/executor/executor.h`, after the `execute(DropTableStmt&)` declaration (line 148), add:

```cpp
    /// Execute a CREATE INDEX statement.
    ExecutionResult execute(const parser::CreateIndexStmt& stmt);

    /// Execute a DROP INDEX statement.
    ExecutionResult execute(const parser::DropIndexStmt& stmt);
```

Add include at top (after existing includes):
```cpp
#include "storage/index_schema.h"
```

- [ ] **Step 2: Implement executeCreateIndex**

In `src/executor/executor.cpp`, after the `execute(DropTableStmt&)` implementation (after line 90), add:

```cpp
ExecutionResult Executor::execute(const parser::CreateIndexStmt& stmt) {
    // Convert AST IndexColumnDef → storage IndexColumn
    std::vector<IndexColumn> columns;
    for (const auto& col : stmt.columns()) {
        IndexSortDir dir = (col.direction == parser::SortDirection::ASC)
            ? IndexSortDir::ASC : IndexSortDir::DESC;
        columns.emplace_back(col.name, dir);
    }

    // Build IndexSchema
    IndexSchema schema(stmt.indexName(), stmt.tableName(),
                       std::move(columns), stmt.isUnique());

    // createIndex does all validation
    auto result = catalog_.createIndex(std::move(schema));
    if (!result.is_ok()) {
        return ExecutionResult::error(result.error().code(),
                                      result.error().message());
    }

    // Persist if storage available
    if (storage_mgr_) {
        const auto* idx = catalog_.getIndex(stmt.indexName());
        storage_mgr_->onCreateIndex(catalog_, *idx);
    }

    return ExecutionResult::empty();
}

ExecutionResult Executor::execute(const parser::DropIndexStmt& stmt) {
    const std::string& name = stmt.indexName();

    // Handle IF EXISTS
    if (!catalog_.hasIndex(name)) {
        if (stmt.hasIfExists()) {
            return ExecutionResult::empty();
        }
        return ExecutionResult::error(
            ErrorCode::INDEX_NOT_FOUND,
            "Index '" + name + "' does not exist");
    }

    auto result = catalog_.dropIndex(name);
    if (!result.is_ok()) {
        return ExecutionResult::error(result.error().code(),
                                      result.error().message());
    }

    if (storage_mgr_) {
        storage_mgr_->onDropIndex(catalog_, name);
    }

    return ExecutionResult::empty();
}
```

- [ ] **Step 3: Add cascade to dropTable**

In `src/executor/executor.cpp`, in the `execute(DropTableStmt&)` method (around lines 82-87), add the cascade call before dropping the table:

```cpp
    // Cascade: drop all associated indexes first
    catalog_.dropAllIndexesForTable(table_name);

    // Drop the table
    if (storage_mgr_) {
        storage_mgr_->onDropTable(table_name, catalog_);
    }
    catalog_.dropTable(table_name);
```

- [ ] **Step 4: Build and run all tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All PASS

- [ ] **Step 5: Commit**

```bash
git add src/executor/executor.h src/executor/executor.cpp
git commit -m "feat(executor): add CREATE INDEX and DROP INDEX execution with cascade"
```

---

### Task 7: CLI — \di Command and Index Statement Dispatch

**Files:**
- Modify: `src/cli/repl.h` (add listIndexes declaration)
- Modify: `src/cli/repl.cpp` (add \di handling, index statement dispatch)

- [ ] **Step 1: Add listIndexes declaration to repl.h**

In `src/cli/repl.h`, after `listTables()` declaration (line 53), add:

```cpp
    /// Lists all indexes, or indexes for a specific table.
    void listIndexes(const std::string& arg);
```

- [ ] **Step 2: Add \di command handling in repl.cpp**

In `src/cli/repl.cpp`, in `handleMetaCommand()` (after the `dt` check around line 103), add:

```cpp
    if (cmd == "di" || cmd.rfind("di ", 0) == 0) {
        std::string arg = (cmd.size() > 3) ? cmd.substr(3) : "";
        listIndexes(arg);
        return true;
    }
```

- [ ] **Step 3: Add \di to help text**

In `showHelp()`, after the `\dt` line (around line 116), add:

```cpp
    std::cout << "  \\di     List indexes\n";
    std::cout << "  \\di t   List indexes for table t\n";
```

- [ ] **Step 4: Implement listIndexes**

Add the implementation after `listTables()` (around line 132):

```cpp
void Repl::listIndexes(const std::string& arg) {
    std::vector<const IndexSchema*> indexes;

    if (!arg.empty()) {
        // Specific table
        if (!catalog_.hasTable(arg)) {
            std::cout << "Table '" << arg << "' not found." << std::endl;
            return;
        }
        indexes = catalog_.getTableIndexes(arg);
        if (indexes.empty()) {
            std::cout << "No indexes found for table '" << arg << "'." << std::endl;
            return;
        }
    } else {
        // All indexes
        auto names = catalog_.listIndexNames();
        if (names.empty()) {
            std::cout << "No indexes found." << std::endl;
            return;
        }
        for (const auto& name : names) {
            indexes.push_back(catalog_.getIndex(name));
        }
    }

    // Header
    std::cout << " Index Name       | Table            | Columns                  | Type  | Unique\n";
    std::cout << "------------------+------------------+--------------------------+-------+-------\n";

    for (const auto* idx : indexes) {
        std::string cols;
        for (size_t i = 0; i < idx->columns().size(); ++i) {
            if (i > 0) cols += ", ";
            cols += idx->columns()[i].name;
            cols += (idx->columns()[i].direction == IndexSortDir::ASC) ? " ASC" : " DESC";
        }
        std::string type_str = (idx->indexType() == IndexType::BTREE) ? "BTREE" : "HASH";
        std::string unique_str = idx->isUnique() ? "Yes" : "No";

        printf(" %-16s | %-16s | %-24s | %-5s | %s\n",
               idx->name().c_str(), idx->tableName().c_str(),
               cols.c_str(), type_str.c_str(), unique_str.c_str());
    }
    std::cout << "(" << indexes.size() << " index" << (indexes.size() == 1 ? "" : "es") << ")" << std::endl;
}
```

Add include at top of repl.cpp:
```cpp
#include "storage/index_schema.h"
```

- [ ] **Step 5: Add index statement dispatch in executeSql**

In `src/cli/repl.cpp`, in `executeSql()`, after the `DropTableStmt` block (around line 166) and before the `InsertStmt` block, add:

```cpp
    else if (auto* create_idx = dynamic_cast<CreateIndexStmt*>(stmt.get())) {
        auto result = executor_.execute(*create_idx);
        if (result.status() == ExecutionResult::Status::OK) {
            std::cout << "CREATE INDEX" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
    else if (auto* drop_idx = dynamic_cast<DropIndexStmt*>(stmt.get())) {
        auto result = executor_.execute(*drop_idx);
        if (result.status() == ExecutionResult::Status::OK) {
            std::cout << "DROP INDEX" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
```

- [ ] **Step 6: Build and run all tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All PASS

- [ ] **Step 7: Manual smoke test**

Run: `./build/seeddb` and try:
```
CREATE TABLE users (id INT, name VARCHAR);
CREATE INDEX idx_name ON users (name);
\di
CREATE UNIQUE INDEX idx_id ON users (id ASC);
\di users
DROP INDEX idx_name;
\di
DROP TABLE users;
\di
```
Expected: All commands succeed, \di shows correct output, DROP TABLE cascades.

- [ ] **Step 8: Commit**

```bash
git add src/cli/repl.h src/cli/repl.cpp
git commit -m "feat(cli): add \\di command and CREATE/DROP INDEX dispatch"
```

---

## Summary

| Task | Component | New/Modified Files | Tests |
|------|-----------|-------------------|-------|
| 1 | IndexSchema + ErrorCodes | 4 files | 3 tests |
| 2 | Catalog Index API | 2 files | 13 tests |
| 3 | Persistence (V2 format) | 3 files | 3 tests |
| 4 | AST Nodes | 2 files | (parser tests in Task 5) |
| 5 | Parser | 3 files | 7 tests |
| 6 | Executor | 2 files | (covered by existing + integration) |
| 7 | CLI (\di) | 2 files | manual smoke test |

**Total: 7 tasks, ~16 files, ~26 unit tests**
