# Disk-Based Query Execution — Design Specification

**Date:** 2026-03-30
**Phase:** 3.3
**Status:** Draft

## 1. Overview

The current Executor operates entirely in memory: `StorageManager::load()` deserializes every row into `Table::rows_` at startup, and all queries iterate that vector. UPDATE/DELETE trigger a full-table checkpoint (drop + recreate + rewrite all rows). This phase replaces the in-memory path with BufferPool-backed disk access.

### Goals

- Queries read rows on-demand via BufferPool — no full-table load at startup
- INSERT/UPDATE/DELETE operate in-place on pages through BufferPool — no checkpoint
- Memory usage stays bounded by `buffer_pool_size` regardless of table size
- Single-table scope (JOINs remain in-memory for now)

### Non-Goals

- Disk-based JOIN execution (future phase)
- B-tree index integration
- WAL / crash recovery
- Overflow pages for large rows

## 2. Architecture Decision — StorageManager-Centric

```
Executor → StorageManager → BufferPool → PageManager → DiskManager → Disk
```

StorageManager owns BufferPool and mediates all data access. The Executor never touches BufferPool, PageManager, or Page directly.

**Why this approach:** Respects existing layering. StorageManager already bridges storage and execution. Keeps Executor clean and testable through StorageManager's API.

## 3. Core Data Structures

### 3.1 TID — Tuple Identifier

```cpp
struct TID {
    uint32_t file_id;    // table file identifier
    uint32_t page_num;   // page number within file
    uint16_t slot_id;    // slot index within page

    bool isValid() const { return file_id != INVALID_FILE_ID; }
};
```

Physical row identifier following PostgreSQL's CTID pattern. Used by the Executor to locate rows for UPDATE/DELETE.

### 3.2 TableIterator — Volcano-Style Interface

```cpp
class TableIterator {
public:
    virtual ~TableIterator() = default;

    /// Advance to next row. Returns false if exhausted.
    virtual bool next() = 0;

    /// Get current row (deserialized). Valid only after next() returns true.
    virtual const Row& currentRow() const = 0;

    /// Get current row's TID. Valid only after next() returns true.
    virtual TID currentTID() const = 0;
};
```

### 3.3 HeapTableIterator — Concrete Implementation

Iterates through all slotted pages of a table via BufferPool:

- **State:** `file_id`, current `page_num`, current `slot_id`, `BufferPool&`, `Schema&`
- **On `next()`:** Advance slot index. Skip deleted slots (size == 0). When all slots on a page are consumed, unpin current page, fetch next page. When no more pages, return false.
- **Row materialization:** Deserializes on demand via `RowSerializer::deserialize()` when `currentRow()` is called (cached until next `next()` call).
- **Pin management:** Current page is pinned while being iterated. Unpinned when moving to the next page or when iterator is destroyed.

## 4. StorageManager Changes

### 4.1 Ownership Chain

```
Before: StorageManager → PageManager → DiskManager
After:  StorageManager → BufferPool → PageManager → DiskManager
```

BufferPool wraps PageManager. StorageManager's constructor changes:

```cpp
// Before
explicit StorageManager(const std::string& data_dir);

// After
StorageManager(const std::string& data_dir, const Config& config);
```

### 4.2 New Public API

| Method | Purpose |
|--------|---------|
| `createIterator(table_name)` | Returns `unique_ptr<TableIterator>` for scanning |
| `insertRow(table_name, row, schema)` | Append row to a page via BufferPool |
| `updateRow(tid, new_row, schema)` | Delete old slot + re-insert new row data |
| `deleteRow(tid)` | Mark slot as deleted, mark page dirty |
| `pageCount(table_name)` | Number of pages in table file |

### 4.3 Removed API

| Method | Reason |
|--------|--------|
| `loadTableRows()` | No longer loading rows into memory |
| `checkpoint()` | Replaced by in-place page writes + BufferPool FlushAll |
| `appendRow()` | Replaced by `insertRow()` which uses BufferPool |

### 4.4 Method Changes

**`load(Catalog& catalog)`** — schema-only startup:
1. Load `catalog.meta` (schemas)
2. Open table files via `page_mgr_.openTableFile()`
3. Create Table entries in Catalog with zero in-memory rows
4. No row deserialization

**`insertRow(table_name, row, schema)`**:
1. Serialize row via `RowSerializer::serialize()`
2. Fetch last page via `buffer_pool_.FetchPage()` — if it has enough free space, insert record
3. If last page is full (or no pages exist), allocate new page via `page_mgr_.allocatePage()`, fetch it, insert
4. Mark page dirty, unpin page

**`updateRow(tid, new_row, schema)`** (delete + re-insert):
1. Fetch page at `tid.page_num` via BufferPool
2. Call `page.deleteRecord(tid.slot_id)` — marks slot as deleted
3. Try `page.insertRecord()` on the same page (may fit if new data is smaller)
4. If it doesn't fit, insert on a new page via `insertRow()` logic
5. Mark page(s) dirty, unpin

**`deleteRow(tid)`**:
1. Fetch page at `tid.page_num` via BufferPool
2. Call `page.deleteRecord(tid.slot_id)`
3. Mark page dirty, unpin

### 4.5 Shutdown

`BufferPool::FlushAll()` runs in `~BufferPool()` destructor, which fires when `StorageManager` is destroyed. Only dirty pages are written back — no full-table rewrite.

## 5. Table Class Simplification

Table becomes a schema-only metadata container:

```cpp
class Table {
public:
    Table(std::string name, Schema schema);
    const std::string& name() const;
    const Schema& schema() const;
private:
    std::string name_;
    Schema schema_;
    // REMOVED: std::vector<Row> rows_
    // REMOVED: insert(), get(), update(), remove(), removeBulk(),
    //          clear(), rowCount(), begin(), end(), iterator typedefs
};
```

All data access goes through `StorageManager::createIterator()`. The `rows_` vector and all row manipulation methods are removed.

## 6. Executor Changes

### 6.1 Removed Members

- `Table* current_table_` — replaced by iterator
- Direct calls to `table->rowCount()`, `table->get(i)`, `table->insert()`, `table->update()`, `table->removeBulk()`

### 6.2 SELECT (Single-Table)

```
1. storage_mgr_->createIterator(table_name) → unique_ptr<TableIterator>
2. while (iter->next()):
     if WHERE matches iter->currentRow():
       result_rows_.push_back(projectRow(iter->currentRow()))
3. Apply DISTINCT, ORDER BY, LIMIT/OFFSET on result_rows_
4. Iterate result_rows_ for output
```

`result_rows_` is still used for materialization (ORDER BY, DISTINCT, aggregates require it), but populated from the iterator instead of `table.rows_`.

### 6.3 INSERT

```
1. Build Row from AST values (unchanged)
2. schema.validateRow(row) (unchanged)
3. storage_mgr_->insertRow(table_name, row, schema)
```

No more `table->insert()` or in-memory storage.

### 6.4 UPDATE

```
1. storage_mgr_->createIterator(table_name)
2. First pass — collect matching TIDs:
     while (iter->next()):
       if WHERE matches iter->currentRow():
         tid_list.push_back(iter->currentTID())
         build new_row from assignments + iter->currentRow()
         save (tid, new_row) pair
3. Second pass — apply mutations:
     for each (tid, new_row):
       storage_mgr_->updateRow(tid, new_row, schema)
```

Two-pass design avoids mutating pages while iterating (re-insert could land on a later page, corrupting iteration).

### 6.5 DELETE

```
1. storage_mgr_->createIterator(table_name)
2. First pass — collect matching TIDs:
     while (iter->next()):
       if WHERE matches iter->currentRow():
         tid_list.push_back(iter->currentTID())
3. Second pass — apply deletes:
     for each tid:
       storage_mgr_->deleteRow(tid)
```

### 6.6 JOIN Queries (Unchanged)

JOINs already materialize both sides into `std::vector<Row>`. The adaptation: instead of reading from `table.rows_`, the JOIN helper materializes from the iterator:

```cpp
// Before
for (size_t i = 0; i < table->rowCount(); ++i) {
    left_rows.push_back(table->get(i));
}

// After
auto iter = storage_mgr_->createIterator(table_name);
while (iter->next()) {
    left_rows.push_back(iter->currentRow());
}
```

## 7. Error Handling

| Scenario | Handling |
|----------|----------|
| Row too large for a single page | `insertRow()` returns false → Executor returns `INTERNAL_ERROR` |
| Page not found during iteration | Skip, log warning, continue (matches current `loadTableRows` behavior) |
| BufferPool exhausted (all frames pinned) | `FetchPage()` returns nullptr → mutation methods return false → Executor returns `INTERNAL_ERROR` |
| File missing for a table | `createIterator()` returns nullptr → Executor returns `TABLE_NOT_FOUND` |
| Dirty page flush failure on shutdown | Log error, best-effort continue in `FlushAll()` |

## 8. Testing Strategy

### 8.1 Test Files

| Test File | Scope |
|-----------|-------|
| **`test_table_iterator.cpp`** (new) | TID struct + HeapTableIterator: empty table, single page, multi-page, deleted slots, TID tracking |
| **`test_storage_manager.cpp`** (update) | Remove `loadTableRows`/`checkpoint` tests. Add: `createIterator()`, `insertRow()`, `updateRow()`, `deleteRow()`, `pageCount()` |
| **`test_executor.cpp`** (update) | Adapt all tests to use StorageManager with temp data dir. Verify end-to-end: INSERT→SELECT, UPDATE→SELECT, DELETE→SELECT |
| **`test_table.cpp`** (update) | Remove row manipulation tests. Keep only `name()` and `schema()` tests |

### 8.2 TDD Order

1. Write `test_table_iterator.cpp` — TID + iterator basics
2. Update `test_storage_manager.cpp` — new disk-based API
3. Update `test_executor.cpp` — end-to-end through new path
4. Update `test_table.cpp` — clean up removed methods

### 8.3 Integration Milestone

Insert 10,000+ rows with a 10-frame buffer pool. Verify:
- All rows readable via SELECT
- UPDATE modifies correct rows
- DELETE removes correct rows
- Memory stays bounded (buffer_pool_size * PAGE_SIZE)

## 9. Files Changed Summary

| File | Action |
|------|--------|
| `src/storage/tid.h` | **NEW** — TID struct |
| `src/storage/table_iterator.h` | **NEW** — TableIterator interface + HeapTableIterator |
| `src/storage/storage_manager.h` | **UPDATE** — new API, BufferPool ownership, Config param |
| `src/storage/storage_manager.cpp` | **UPDATE** — rewrite for disk-based access |
| `src/storage/table.h` | **UPDATE** — remove rows_ and row methods |
| `src/executor/executor.h` | **UPDATE** — remove current_table_, update method signatures |
| `src/executor/executor.cpp` | **UPDATE** — iterator-based execution |
| `src/cli/main.cpp` | **UPDATE** — pass Config to StorageManager |
| `src/storage/CMakeLists.txt` | **UPDATE** — add new source files |
| `src/executor/CMakeLists.txt` | **UPDATE** — add storage dependency if needed |
| `tests/unit/storage/test_table_iterator.cpp` | **NEW** |
| `tests/unit/storage/test_storage_manager.cpp` | **UPDATE** |
| `tests/unit/executor/test_executor.cpp` | **UPDATE** |
| `tests/unit/storage/test_table.cpp` | **UPDATE** |
| `tests/CMakeLists.txt` | **UPDATE** — add new test file |
