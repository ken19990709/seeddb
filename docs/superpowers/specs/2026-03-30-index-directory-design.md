# 3.4 Index Directory Extension — Design Spec

## Overview

Phase 3.4 builds the metadata infrastructure for B+ tree indexes. It introduces index schema definitions, catalog management, parser support, and persistence — without implementing the actual index data structure (deferred to Phase 3.5).

**Estimated duration:** 0.5 weeks

**Prerequisite:** Phase 3.3 (disk-based query execution) is substantially complete.

---

## Architecture Decision

**Pattern:** Catalog as unified API with clean internal boundaries.

The Catalog remains the single entry point for both table and index management. Internally, tables and indexes are stored in separate maps. This avoids the complexity of cross-class coordination (separate IndexCatalog) while keeping data boundaries clean for future refactoring.

**Why not a separate IndexCatalog class?** For SeedDB's scope (tables + indexes + eventual constraints), a unified Catalog is sufficient. The internal separation (`tables_` map vs `indexes_` map) means we can extract an IndexCatalog later without changing the public API.

---

## C3-1 IndexSchema Definition

### New file: `src/storage/index_schema.h`

Requires `#include <string>`, `#include <vector>`.

```cpp
namespace seeddb {

/// Index type enumeration
enum class IndexType {
    BTREE,   // Default — Phase 3.5 implements
    HASH,    // Future
};

/// Sort direction for a single column in the index.
/// Separate from parser::SortDirection to maintain layer independence.
/// Conversion happens at the executor boundary.
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
    const std::string& name() const;
    const std::string& tableName() const;
    const std::vector<IndexColumn>& columns() const;
    bool isUnique() const;
    bool isPrimary() const;
    IndexType indexType() const;

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
```

### Design decisions

| Decision | Rationale |
|----------|-----------|
| `IndexColumn` bundles name + direction | Avoids parallel vectors; single unit of meaning |
| `is_primary_` flag | Reserved for PRIMARY KEY constraint support |
| `IndexType` enum | Reserved for future HASH indexes; only BTREE used now |
| Global index name uniqueness | Follows SQLite/MySQL model; simpler than table-scoped names |
| Separate `IndexSortDir` from `parser::SortDirection` | Maintains storage/parser layer independence; conversion at executor boundary |

### `IndexSortDir` ↔ `SortDirection` conversion

```cpp
// In executor, when building IndexSchema from AST:
IndexSortDir to_storage_dir(parser::SortDirection d) {
    return (d == parser::SortDirection::ASC) ? IndexSortDir::ASC : IndexSortDir::DESC;
}
```

---

## C3-2 Catalog Extension

### New ErrorCode

Add to `src/common/error.h` after `DUPLICATE_COLUMN = 204`:

```cpp
DUPLICATE_INDEX = 205,
REDUNDANT_INDEX = 206,
```

### API surface

Add to `Catalog` class in `src/storage/catalog.h`. Requires `#include "storage/index_schema.h"`.

```cpp
class Catalog {
public:
    // ====== Index Management ======

    /// Create a new index. Validates: index name unique, table exists,
    /// columns exist in table, no exact duplicate of existing index.
    /// @return ok() on success, err with specific ErrorCode on validation failure.
    Result<void> createIndex(IndexSchema index);

    /// Drop an index by name.
    /// @return ok() if dropped, err(INDEX_NOT_FOUND) if not found.
    Result<void> dropIndex(const std::string& name);

    /// Get index by name. Returns nullptr if not found.
    const IndexSchema* getIndex(const std::string& name) const;

    /// Check if an index exists.
    bool hasIndex(const std::string& name) const;

    /// Get all indexes for a given table.
    std::vector<const IndexSchema*> getTableIndexes(const std::string& table_name) const;

    /// List all index names in the database.
    std::vector<std::string> listIndexNames() const;

    /// Drop all indexes associated with a table. Called by dropTable().
    void dropAllIndexesForTable(const std::string& table_name);

    /// Returns the number of indexes.
    size_t indexCount() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
    std::unordered_map<std::string, IndexSchema> indexes_;  // index_name → IndexSchema
};
```

### Validation rules in `createIndex()`

`createIndex()` performs all validation and returns specific error codes:

1. Index name already exists → `DUPLICATE_INDEX`
2. Referenced table does not exist → `TABLE_NOT_FOUND`
3. Column not found in table schema → `COLUMN_NOT_FOUND`
4. Redundant index (identical columns + directions on same table) → `REDUNDANT_INDEX`

The executor calls `createIndex()` and propagates any error directly — no duplication of validation logic.

### `dropTable()` enhancement

Existing `dropTable()` must call `dropAllIndexesForTable()` before erasing the table entry.

### Storage choice

`indexes_` uses value semantics (`std::unordered_map<std::string, IndexSchema>`) rather than `unique_ptr` because `IndexSchema` is a plain data holder — no need for pointer stability or polymorphism.

### Note on `bool` vs `Result<void>`

Existing `createTable()` and `dropTable()` return `bool` (predating the coding standard). New index methods use `Result<void>` per the coding standard (section 3.1). The table methods are not changed to avoid unnecessary churn.

---

## C3-3 catalog.meta Persistence Format

### Current binary format (V1)

The existing `catalog.meta` uses a binary format with no version field:

```
[4 bytes uint32_t: table_count]
For each table:
  [4 bytes uint32_t: name_length]
  [name_length bytes: table name (UTF-8)]
  [4 bytes uint32_t: column_count]
  For each column:
    [4 bytes uint32_t: col_name_length]
    [col_name_length bytes: column name]
    [1 byte uint8_t: LogicalTypeId]
    [1 byte uint8_t: nullable (0=not nullable, 1=nullable)]
```

### V2 binary format

Add a version header and an indexes section after tables:

```
[4 bytes uint32_t: version = 2]
[4 bytes uint32_t: table_count]
For each table:                        // (same as V1)
  [4 bytes uint32_t: name_length]
  [name_length bytes: table name]
  [4 bytes uint32_t: column_count]
  For each column:
    [4 bytes uint32_t: col_name_length]
    [col_name_length bytes: column name]
    [1 byte uint8_t: LogicalTypeId]
    [1 byte uint8_t: nullable]
[4 bytes uint32_t: index_count]        // NEW
For each index:                        // NEW
  [4 bytes uint32_t: name_length]
  [name_length bytes: index name]
  [4 bytes uint32_t: table_name_length]
  [table_name_length bytes: table name]
  [4 bytes uint32_t: column_count]
  For each column:
    [4 bytes uint32_t: col_name_length]
    [col_name_length bytes: column name]
    [1 byte uint8_t: IndexSortDir (0=ASC, 1=DESC)]
  [1 byte uint8_t: is_unique (0/1)]
  [1 byte uint8_t: is_primary (0/1)]
  [1 byte uint8_t: IndexType (0=BTREE, 1=HASH)]
```

### Backward compatibility

- V1 files have no version header — the first 4 bytes are `table_count` (typically a small number like 0-10).
- Detection: if the first 4 bytes are 0 or 1, treat as a version field (V2+). If ≥ 2 and looks like a reasonable table count, treat as V1 (no version field).
- Alternative (simpler): always write V2 with version=2. On load, peek at first 4 bytes: if == 2, it's V2; otherwise, seek back to position 0 and read as V1.
- When no indexes section is present (V1), indexes are loaded as empty.

### StorageManager ownership model

**StorageManager does NOT hold an `indexes_` member.** Index data flows through StorageManager only during serialization/deserialization:

- **Save path:** `StorageManager::onCreateIndex()` / `onDropIndex()` accept index data as parameters (from Catalog) and write it to disk.
- **Load path:** `StorageManager::loadCatalogMeta()` reads index entries from disk and populates a temporary collection. `StorageManager::load()` then passes each index to `catalog.createIndex()`.

This matches the existing table pattern where `schemas_` in StorageManager is a persistence cache, but avoids a parallel cache for indexes since the StorageManager doesn't need index schemas for any disk operation.

### StorageManager changes

```cpp
class StorageManager {
public:
    // ... existing methods ...

    /// Persist a newly created index.
    bool onCreateIndex(const IndexSchema& index);

    /// Persist index deletion.
    bool onDropIndex(const std::string& name);

private:
    // ... existing members ...
    // No indexes_ member needed — data comes from Catalog on save,
    // goes to Catalog on load.
};
```

`saveCatalogMeta()` now requires access to the Catalog's indexes. Two options:

1. Pass Catalog reference to `saveCatalogMeta()` (preferred — minimal coupling)
2. Accept a `const std::unordered_map<std::string, IndexSchema>&` parameter

Option 1 is cleaner since `load()` already takes `Catalog&`.

```cpp
bool saveCatalogMeta(const Catalog& catalog);  // modified signature
```

### Persistence failure semantics

If `saveCatalogMeta()` fails (disk full, permission error), the in-memory state in Catalog is already updated. This matches the existing behavior for table creation. Future phases should add transactional semantics.

---

## C3-4 CREATE INDEX Parsing

### Syntax

```
CREATE [UNIQUE] INDEX index_name ON table_name (column [ASC|DESC] [, ...])
```

### NodeType enum extension

Add to `NodeType` in `src/parser/ast.h`:

```cpp
STMT_CREATE_INDEX,
STMT_DROP_INDEX,
```

### New AST nodes (in `namespace seeddb::parser`)

```cpp
namespace seeddb {
namespace parser {

/// Index column definition at AST level
struct IndexColumnDef {
    std::string name;
    SortDirection direction;  // Reuse existing SortDirection enum
};

/// CREATE INDEX statement
class CreateIndexStmt : public Stmt {
public:
    CreateIndexStmt(std::string index_name, std::string table_name,
                    std::vector<IndexColumnDef> columns, bool is_unique);

    NodeType type() const override { return NodeType::STMT_CREATE_INDEX; }
    std::string toString() const override;

    const std::string& indexName() const;
    const std::string& tableName() const;
    const std::vector<IndexColumnDef>& columns() const;
    bool isUnique() const;

private:
    std::string index_name_;
    std::string table_name_;
    std::vector<IndexColumnDef> columns_;
    bool is_unique_;
};

}  // namespace parser
}  // namespace seeddb
```

### Parser dispatch (parseStatement modification)

The existing `parseStatement()` unconditionally routes `CREATE` tokens to `parseCreateTable()`. This must change:

```
parseStatement():
  on CREATE:
    save position (for backtrack)
    consume CREATE
    if current() == UNIQUE:
      consume UNIQUE
      set unique_flag = true
    if current() == INDEX:
      → parseCreateIndex(unique_flag)
    else if current() == TABLE:
      → parseCreateTable()
    else:
      error "expected TABLE or INDEX after CREATE"
  on DROP:
    consume DROP
    if current() == TABLE:
      → parseDropTable()
    else if current() == INDEX:
      → parseDropIndex()
    else:
      error "expected TABLE or INDEX after DROP"
```

Note: `parseCreateTable()` currently consumes `CREATE TABLE` internally. It must be refactored so that `parseStatement()` consumes `CREATE` and dispatches based on the next keyword (`TABLE` vs `INDEX`). The same applies to `DROP`.

### Parser method

```cpp
Result<std::unique_ptr<CreateIndexStmt>> parseCreateIndex();
```

Takes an optional `bool is_unique` parameter (already determined by the dispatcher).

Parsing steps:
1. Expect `INDEX` (already consumed CREATE and optional UNIQUE by dispatcher)
2. Read index name (identifier)
3. Expect `ON`
4. Read table name (identifier)
5. Expect `(`
6. Parse comma-separated column list with optional ASC/DESC
7. Expect `)`

### Lexer tokens

Ensure `INDEX` is recognized as a keyword. `ON`, `ASC`, `DESC` likely already exist — verify and add if missing.

---

## C3-5 DROP INDEX Parsing

### Syntax

```
DROP INDEX index_name [IF EXISTS]
```

### New AST node (in `namespace seeddb::parser`)

```cpp
namespace seeddb {
namespace parser {

class DropIndexStmt : public Stmt {
public:
    DropIndexStmt(std::string index_name, bool if_exists);

    NodeType type() const override { return NodeType::STMT_DROP_INDEX; }
    std::string toString() const override;

    const std::string& indexName() const;
    bool hasIfExists() const;

private:
    std::string index_name_;
    bool if_exists_;
};

}  // namespace parser
}  // namespace seeddb
```

### Parser method

```cpp
Result<std::unique_ptr<DropIndexStmt>> parseDropIndex();
```

Takes no special parameters (DROP already consumed by dispatcher).

Parsing steps:
1. Expect `INDEX` (already consumed DROP by dispatcher)
2. Read index name (identifier)
3. Check for optional `IF EXISTS`

---

## Executor Integration

### New execute() overloads

Add to `src/executor/executor.h`:

```cpp
ExecutionResult execute(const parser::CreateIndexStmt& stmt);
ExecutionResult execute(const parser::DropIndexStmt& stmt);
```

### Dispatch integration

The main dispatch loop (wherever `NodeType` is switched to call the appropriate `execute()` overload) needs new branches:

```cpp
case NodeType::STMT_CREATE_INDEX:
    return execute(static_cast<const CreateIndexStmt&>(stmt));
case NodeType::STMT_DROP_INDEX:
    return execute(static_cast<const DropIndexStmt&>(stmt));
```

### executeCreateIndex

```
Input: CreateIndexStmt
  1. Convert AST IndexColumnDef → storage IndexColumn (apply IndexSortDir conversion)
  2. Build IndexSchema from converted data
  3. Call catalog.createIndex(schema)
  4. If Result is err → propagate error as ExecutionResult
  5. Call storage_manager->onCreateIndex(schema)
  6. Return success message: "Index 'xxx' created on table 'yyy'"
```

All validation (table exists, columns exist, name unique, no redundant index) is done inside `catalog.createIndex()`. The executor does NOT duplicate these checks.

### executeDropIndex

```
Input: DropIndexStmt
  1. If !IF_EXISTS and !catalog.hasIndex(name) → error INDEX_NOT_FOUND
  2. If IF_EXISTS and !catalog.hasIndex(name) → success (no-op)
  3. Call catalog.dropIndex(name)
  4. Call storage_manager->onDropIndex(name)
  5. Return success message: "Index 'xxx' dropped"
```

Note: `dropIndex()` returns `Result<void>` so we check `hasIndex()` first for the IF_EXISTS logic, then call `dropIndex()` which should succeed since we verified existence.

### dropTable enhancement

```
Existing dropTable flow (in executor):
  1. Validate table exists
  2. catalog.dropAllIndexesForTable(name)   // NEW
  3. catalog.dropTable(name)
  4. storage_manager->onDropTable(name)
```

`saveCatalogMeta()` is called inside `onDropTable()`, which now also persists the index deletions.

### Error messages

All errors use the existing `Result<T>` pattern:

| Condition | ErrorCode | Error message |
|-----------|-----------|---------------|
| Index name duplicate | `DUPLICATE_INDEX` | `"Index 'xxx' already exists"` |
| Table not found | `TABLE_NOT_FOUND` | `"Table 'xxx' does not exist"` |
| Column not in table | `COLUMN_NOT_FOUND` | `"Column 'xxx' does not exist in table 'yyy'"` |
| Index not found | `INDEX_NOT_FOUND` | `"Index 'xxx' does not exist"` |
| Redundant index | `REDUNDANT_INDEX` | `"An equivalent index already exists on table 'xxx'"` |

---

## \di Command (psql-style)

Handled at CLI layer, not in the parser.

### Commands

- `\di` — list all indexes
- `\di <table_name>` — list indexes for a specific table

### Output format

```
 Index Name  | Table   | Columns        | Type  | Unique
-------------+---------+----------------+-------+--------
 idx_name    | users   | name ASC       | BTREE | No
 pk_users_id | users   | id ASC         | BTREE | Yes
```

### Implementation

Query `catalog.listIndexNames()` or `catalog.getTableIndexes()`, format with column alignment.

---

## Test Plan

### File: `tests/unit/test_index_catalog.cpp`

```
1. IndexSchema basics
   - Construction and accessor correctness
   - Multi-column index with mixed ASC/DESC
   - toString() format

2. Catalog index CRUD
   - createIndex / getIndex / hasIndex
   - createIndex: duplicate name → DUPLICATE_INDEX
   - createIndex: table not found → TABLE_NOT_FOUND
   - createIndex: column not in table → COLUMN_NOT_FOUND
   - createIndex: redundant index → REDUNDANT_INDEX
   - dropIndex: exists → ok
   - dropIndex: not found → INDEX_NOT_FOUND
   - dropAllIndexesForTable
   - dropTable cascades to indexes
   - getTableIndexes / listIndexNames
   - indexCount

3. Parser
   - CREATE INDEX basic
   - CREATE UNIQUE INDEX
   - CREATE INDEX multi-column with ASC/DESC
   - DROP INDEX
   - DROP INDEX IF EXISTS
   - Syntax errors

4. Persistence
   - Create index → save → reload → index restored
   - Drop index → save → reload → index gone
   - V1 format file loads with empty indexes (backward compat)

5. End-to-end (via test script)
   - CREATE TABLE + CREATE INDEX → \di lists correctly
   - DROP TABLE → associated indexes removed
```

### Integration script: `test_index.sh`

Shell script that pipes SQL commands through the CLI and validates output.

---

## Milestone

The system should be able to:
1. Parse `CREATE INDEX` and `DROP INDEX` statements
2. Store index metadata in the catalog
3. Persist and restore index definitions across restarts
4. List indexes via `\di` command
5. Cascade index deletion when dropping a table
