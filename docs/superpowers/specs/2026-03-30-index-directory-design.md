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

```cpp
namespace seeddb {

/// Index type enumeration
enum class IndexType {
    BTREE,   // Default — Phase 3.5 implements
    HASH,    // Future
};

/// Sort direction for a single column in the index
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

---

## C3-2 Catalog Extension

### API surface

Add to `Catalog` class in `src/storage/catalog.h`:

```cpp
class Catalog {
public:
    // ====== Index Management ======

    /// Create a new index. Validates: index name unique, table exists,
    /// columns exist in table, no exact duplicate of existing index.
    /// @return true if created, false on validation failure.
    bool createIndex(IndexSchema index);

    /// Drop an index by name.
    /// @return true if dropped, false if not found.
    bool dropIndex(const std::string& name);

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

1. Index name must not already exist in `indexes_`
2. Referenced table must exist in `tables_`
3. Every column in the index must exist in the table's schema
4. No two indexes on the same table may have identical column list + direction (redundant check)

### `dropTable()` enhancement

Existing `dropTable()` must call `dropAllIndexesForTable()` before erasing the table entry.

### Storage choice

`indexes_` uses value semantics (`std::unordered_map<std::string, IndexSchema>`) rather than `unique_ptr` because `IndexSchema` is a plain data holder — no need for pointer stability or polymorphism.

---

## C3-3 catalog.meta Persistence Format

### Version 2 format

```json
{
  "version": 2,
  "tables": {
    "users": {
      "columns": [
        {"name": "id", "type": "INTEGER", "nullable": false},
        {"name": "name", "type": "VARCHAR", "nullable": true}
      ]
    }
  },
  "indexes": {
    "idx_users_name": {
      "table": "users",
      "columns": [
        {"name": "name", "direction": "ASC"}
      ],
      "unique": false,
      "primary": false,
      "type": "BTREE"
    }
  }
}
```

### Backward compatibility

- V1 files (no `version` field or `version: 1`) have no `indexes` section — loaded as empty.
- On first write after upgrade, the file is saved in V2 format.

### StorageManager changes

Add two new hook methods:

```cpp
bool onCreateIndex(const IndexSchema& index);
bool onDropIndex(const std::string& name);
```

Both call `saveCatalogMeta()` after modifying the in-memory index set.

The `loadCatalogMeta()` method is extended to parse the `indexes` section and populate a new `indexes_` member:

```cpp
std::unordered_map<std::string, IndexSchema> indexes_;  // alongside schemas_
```

On `load()`, indexes are transferred to the Catalog:

```cpp
bool load(Catalog& catalog);  // now also calls catalog.createIndex() for each loaded index
```

---

## C3-4 CREATE INDEX Parsing

### Syntax

```
CREATE [UNIQUE] INDEX index_name ON table_name (column [ASC|DESC] [, ...])
```

### New AST node

```cpp
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
```

### Parser method

```cpp
Result<std::unique_ptr<CreateIndexStmt>> parseCreateIndex();
```

Parsing steps:
1. Consume `CREATE`
2. Check for optional `UNIQUE` keyword
3. Expect `INDEX`
4. Read index name (identifier)
5. Expect `ON`
6. Read table name (identifier)
7. Expect `(`
8. Parse comma-separated column list with optional ASC/DESC
9. Expect `)`

### Lexer tokens

Ensure `INDEX`, `ON`, `ASC`, `DESC` are recognized as keywords. `ON` likely already exists for JOIN clauses — reuse it.

---

## C3-5 DROP INDEX Parsing

### Syntax

```
DROP INDEX index_name [IF EXISTS]
```

### New AST node

```cpp
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
```

### Parser method

```cpp
Result<std::unique_ptr<DropIndexStmt>> parseDropIndex();
```

---

## Executor Integration

### executeCreateIndex

```
Input: CreateIndexStmt
  1. Validate table exists (catalog.hasTable)
  2. Validate each column exists in table schema
  3. Validate index name not already taken (catalog.hasIndex)
  4. Check for redundant indexes (same table + same columns + same directions)
  5. Build IndexSchema from AST data
  6. catalog.createIndex(schema)
  7. storage_manager->onCreateIndex(schema)
  8. Return success message
```

### executeDropIndex

```
Input: DropIndexStmt
  1. If !IF_EXISTS and index not found → error
  2. If IF_EXISTS and index not found → no-op success
  3. catalog.dropIndex(name)
  4. storage_manager->onDropIndex(name)
  5. Return success message
```

### dropTable enhancement

```
Existing dropTable flow:
  1. Validate table exists
  2. catalog.dropAllIndexesForTable(name)   // NEW
  3. catalog.dropTable(name)
  4. storage_manager->onDropTable(name)
```

### Error messages

All errors use the existing `Result<T>` pattern:

| Condition | Error message |
|-----------|---------------|
| Table not found | `"Table 'xxx' does not exist"` |
| Column not in table | `"Column 'xxx' does not exist in table 'yyy'"` |
| Index name duplicate | `"Index 'xxx' already exists"` |
| Index not found | `"Index 'xxx' does not exist"` |
| Redundant index | `"An equivalent index already exists on table 'xxx'"` |

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
   - createIndex: duplicate name → false
   - createIndex: table not found → false
   - createIndex: column not in table → false
   - dropIndex: exists → true
   - dropIndex: not found → false
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
   - V1 format file loads with empty indexes

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
