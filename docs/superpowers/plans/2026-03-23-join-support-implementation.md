# Phase 2.5 JOIN Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement multi-table JOIN support for SeedDB enabling CROSS, INNER, LEFT, and RIGHT joins with nested-loop execution.

**Architecture:** Add JoinClause AST node with JoinType enum. Parser handles both comma-separated and explicit JOIN syntax. Executor uses simple nested-loop join algorithm with cross product, condition filtering, and NULL padding for outer joins.

**Tech Stack:** C++17, Catch2 test framework, existing parser/executor infrastructure

**Spec Reference:** `docs/superpowers/specs/2026-03-23-join-support-design.md`

---

## File Structure

### New Files
None - all changes are modifications to existing files

### Modified Files

**Parser Layer:**
- `src/parser/ast.h` - Add JoinClause class, JoinType enum, modify SelectStmt
- `src/parser/ast.cpp` - Implement JoinClause::toString()
- `src/parser/parser.h` - Add parseJoinClause() method
- `src/parser/parser.cpp` - Implement JOIN parsing logic

**Executor Layer:**
- `src/executor/executor.h` - Add join execution methods
- `src/executor/executor.cpp` - Implement join execution logic

**Tests:**
- `tests/unit/parser/test_parser.cpp` - Parser tests for JOIN syntax
- `tests/unit/executor/test_executor.cpp` - Executor tests for join execution

---

## Phase 2.5a: Comma-Separated Tables (Implicit CROSS JOIN)

### Task 1: Add JoinType Enum and JoinClause AST Node

**Files:**
- Modify: `src/parser/ast.h:16-41` (NodeType enum)
- Modify: `src/parser/ast.h:43-50` (new JoinType enum)
- Modify: `src/parser/ast.h:489-506` (after TableRef class)

- [ ] **Step 1: Add JOIN_CLAUSE to NodeType enum**

```cpp
// In ast.h, in NodeType enum (around line 38)
enum class NodeType {
    // Statements
    STMT_CREATE_TABLE = 0,
    STMT_DROP_TABLE,
    STMT_INSERT,
    STMT_SELECT,
    STMT_UPDATE,
    STMT_DELETE,
    // Expressions
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_COLUMN_REF,
    EXPR_IS_NULL,
    EXPR_AGGREGATE,
    EXPR_CASE,
    EXPR_IN,
    EXPR_BETWEEN,
    EXPR_LIKE,
    EXPR_COALESCE,
    EXPR_NULLIF,
    EXPR_FUNCTION_CALL,
    // Definitions
    COLUMN_DEF,
    TABLE_REF,
    JOIN_CLAUSE  // NEW: Join clause node
};
```

- [ ] **Step 2: Add JoinType enum**

```cpp
// In ast.h, after AggregateType enum (around line 50)
/// Join type enumeration
enum class JoinType {
    CROSS,      // CROSS JOIN or comma-separated
    INNER,      // INNER JOIN ... ON
    LEFT,       // LEFT [OUTER] JOIN ... ON
    RIGHT       // RIGHT [OUTER] JOIN ... ON
};
```

- [ ] **Step 3: Add JoinClause class declaration**

```cpp
// In ast.h, after TableRef class (around line 507)
/// Join clause (represents a table join)
class JoinClause : public ASTNode {
public:
    JoinClause(JoinType type, std::unique_ptr<TableRef> table, 
               std::unique_ptr<Expr> condition = nullptr)
        : join_type_(type), table_(std::move(table)), 
          condition_(std::move(condition)) {}
    
    NodeType type() const override { return NodeType::JOIN_CLAUSE; }
    std::string toString() const override;
    
    JoinType joinType() const { return join_type_; }
    const TableRef* table() const { return table_.get(); }
    const Expr* condition() const { return condition_.get(); }
    bool hasCondition() const { return condition_ != nullptr; }
    
private:
    JoinType join_type_;
    std::unique_ptr<TableRef> table_;
    std::unique_ptr<Expr> condition_;  // nullptr for CROSS JOIN
};
```

- [ ] **Step 4: Update SelectStmt class to add joins_ vector**

```cpp
// In ast.h, in SelectStmt class (around line 556)
// Add these methods in the public section:
    const auto& joins() const { return joins_; }
    bool hasJoins() const { return !joins_.empty(); }
    void addJoin(std::unique_ptr<JoinClause> join) { 
        joins_.push_back(std::move(join)); 
    }

// Add this private member:
private:
    // ... existing members ...
    std::vector<std::unique_ptr<JoinClause>> joins_;  // Additional tables with join type
```

- [ ] **Step 5: Build and verify compilation**

Run: `cd /home/zxx/seeddb/build && make -j4`
Expected: Compilation succeeds (toString() not implemented yet)

- [ ] **Step 6: Commit AST changes**

```bash
git add src/parser/ast.h
git commit -m "feat(parser): add JoinClause AST node and JoinType enum

- Add JOIN_CLAUSE to NodeType enum
- Add JoinType enum (CROSS, INNER, LEFT, RIGHT)
- Add JoinClause class with table reference and optional condition
- Add joins_ vector to SelectStmt for multi-table support"
```

### Task 2: Implement JoinClause::toString()

**Files:**
- Modify: `src/parser/ast.cpp:155-158` (after TableRef::toString())

- [ ] **Step 1: Write failing test for JoinClause toString**

```cpp
// In tests/unit/parser/test_ast.cpp (add at end)
TEST_CASE("JoinClause toString for CROSS JOIN", "[ast][join]") {
    auto table_ref = std::make_unique<parser::TableRef>("users", "u");
    auto join = std::make_unique<parser::JoinClause>(
        parser::JoinType::CROSS, 
        std::move(table_ref)
    );
    
    REQUIRE(join->toString() == "CROSS JOIN users AS u");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/parser/test_parser "[ast][join]" -v`
Expected: FAIL - toString() not implemented

- [ ] **Step 3: Implement JoinClause::toString()**

```cpp
// In ast.cpp, after TableRef::toString() (around line 158)
std::string JoinClause::toString() const {
    std::string result;
    
    // Add join type
    switch (join_type_) {
        case JoinType::CROSS:
            result += "CROSS JOIN ";
            break;
        case JoinType::INNER:
            result += "INNER JOIN ";
            break;
        case JoinType::LEFT:
            result += "LEFT JOIN ";
            break;
        case JoinType::RIGHT:
            result += "RIGHT JOIN ";
            break;
    }
    
    // Add table reference
    result += table_->toString();
    
    // Add ON condition if present
    if (condition_) {
        result += " ON " + condition_->toString();
    }
    
    return result;
}
```

- [ ] **Step 4: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/parser/test_parser "[ast][join]" -v`
Expected: PASS

- [ ] **Step 5: Commit toString implementation**

```bash
git add src/parser/ast.cpp tests/unit/parser/test_parser.cpp
git commit -m "feat(parser): implement JoinClause::toString()

- Support all join types (CROSS, INNER, LEFT, RIGHT)
- Include table reference and optional ON condition
- Add test for CROSS JOIN toString"
```

### Task 3: Update SelectStmt::toString() to Include Joins

**Files:**
- Modify: `src/parser/ast.cpp:176-193` (SelectStmt::toString())

- [ ] **Step 1: Write failing test for SelectStmt with joins**

```cpp
// In tests/unit/parser/test_parser.cpp
TEST_CASE("SelectStmt toString with comma-separated tables", "[ast][join]") {
    parser::Lexer lexer("SELECT * FROM a, b");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(stmt->toString() == "SELECT * FROM a, b");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/parser/test_parser "[ast][join]" -v`
Expected: FAIL - joins not included in toString()

- [ ] **Step 3: Update SelectStmt::toString() to include joins**

```cpp
// In ast.cpp, in SelectStmt::toString() (around line 191)
    if (from_table_) {
        result += " FROM " + from_table_->toString();
        
        // Add joins
        for (const auto& join : joins_) {
            if (join->joinType() == JoinType::CROSS && !join->hasCondition()) {
                // Comma-separated syntax
                result += ", " + join->table()->toString();
            } else {
                // Explicit JOIN syntax
                result += " " + join->toString();
            }
        }
    }
```

- [ ] **Step 4: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/parser/test_parser "[ast][join]" -v`
Expected: PASS

- [ ] **Step 5: Commit SelectStmt changes**

```bash
git add src/parser/ast.cpp tests/unit/parser/test_parser.cpp
git commit -m "feat(parser): include joins in SelectStmt::toString()

- Support comma-separated table syntax
- Support explicit JOIN syntax
- Add test for comma-separated tables"
```

### Task 4: Implement Comma-Separated Table Parsing

**Files:**
- Modify: `src/parser/parser.h:76` (add parseJoinClause method)
- Modify: `src/parser/parser.cpp:297-306` (parseSelect FROM clause)

- [ ] **Step 1: Write failing test for comma-separated tables**

```cpp
// In tests/unit/parser/test_parser.cpp
TEST_CASE("Parse comma-separated tables", "[parser][join]") {
    parser::Lexer lexer("SELECT * FROM a, b");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(stmt->fromTable()->name() == "a");
    REQUIRE(stmt->hasJoins());
    REQUIRE(stmt->joins().size() == 1);
    REQUIRE(stmt->joins()[0]->joinType() == parser::JoinType::CROSS);
    REQUIRE(stmt->joins()[0]->table()->name() == "b");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/parser/test_parser "[parser][join]" -v`
Expected: FAIL - parser doesn't accept comma after table

- [ ] **Step 3: Add parseJoinClause declaration**

```cpp
// In parser.h, in private section (around line 76)
    // Table reference and JOIN parsing
    Result<std::unique_ptr<TableRef>> parseTableRef();
    Result<std::unique_ptr<JoinClause>> parseJoinClause();  // NEW
```

- [ ] **Step 4: Implement comma-separated table parsing in parseSelect()**

```cpp
// In parser.cpp, in parseSelect(), after parsing first table (around line 306)
    stmt->setFromTable(std::move(table.value()));
    
    // NEW: Parse comma-separated tables
    while (match(TokenType::COMMA)) {
        auto next_table = parseTableRef();
        if (!next_table.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(next_table.error());
        }
        // Create CROSS JOIN clause for comma-separated table
        stmt->addJoin(std::make_unique<JoinClause>(
            JoinType::CROSS,
            std::move(next_table.value())
        ));
    }
    
    // Optional WHERE
    if (match(TokenType::WHERE)) {
```

- [ ] **Step 5: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/parser/test_parser "[parser][join]" -v`
Expected: PASS

- [ ] **Step 6: Add test for three tables**

```cpp
TEST_CASE("Parse three comma-separated tables", "[parser][join]") {
    parser::Lexer lexer("SELECT * FROM a, b, c");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(stmt->joins().size() == 2);
    REQUIRE(stmt->joins()[0]->table()->name() == "b");
    REQUIRE(stmt->joins()[1]->table()->name() == "c");
}
```

- [ ] **Step 7: Run test**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/parser/test_parser "[parser][join]" -v`
Expected: PASS

- [ ] **Step 8: Commit comma-separated table parsing**

```bash
git add src/parser/parser.h src/parser/parser.cpp tests/unit/parser/test_parser.cpp
git commit -m "feat(parser): support comma-separated tables in FROM clause

- Parse multiple tables separated by commas
- Create CROSS JOIN clause for each additional table
- Support any number of tables (2, 3, 4+)
- Add parser tests for comma-separated tables"
```

### Task 5: Add Executor Join Helper Methods

**Files:**
- Modify: `src/executor/executor.h:186-243` (add join methods)

- [ ] **Step 1: Add join method declarations to executor.h**

```cpp
// In executor.h, in private section (around line 243)
    // =========================================================================
    // Join Helper Methods (NEW)
    // =========================================================================

    /// Prepare a multi-table join query.
    /// @param stmt The SELECT statement.
    /// @return true if prepared successfully.
    bool prepareJoinSelect(const parser::SelectStmt& stmt);

    /// Execute cross join (Cartesian product).
    /// @param tables The tables to join.
    /// @param stmt The SELECT statement.
    void executeCrossJoin(const std::vector<Table*>& tables,
                          const parser::SelectStmt& stmt);

    /// Combine two rows into one.
    /// @param left Left row.
    /// @param right Right row.
    /// @param left_schema Left table schema.
    /// @param right_schema Right table schema.
    /// @return Combined row.
    Row combineRows(const Row& left, const Row& right,
                    const Schema& left_schema, const Schema& right_schema) const;

    /// Build combined schema for join result.
    /// @param schemas Vector of table schemas.
    /// @param table_aliases Vector of table aliases.
    /// @return Combined schema with prefixed column names for duplicates.
    Schema buildJoinSchema(const std::vector<Schema>& schemas,
                           const std::vector<std::string>& table_aliases) const;

    /// Make a NULL row for a schema.
    /// @param schema The schema.
    /// @return Row with all NULL values.
    Row makeNullRow(const Schema& schema) const;
```

- [ ] **Step 2: Build and verify compilation**

Run: `cd /home/zxx/seeddb/build && make -j4`
Expected: Compilation fails (methods not implemented)

- [ ] **Step 3: Implement makeNullRow() helper**

```cpp
// In executor.cpp, at end of file (before closing namespace)
// =============================================================================
// Join Helper Methods
// =============================================================================

Row Executor::makeNullRow(const Schema& schema) const {
    std::vector<Value> values;
    values.reserve(schema.columnCount());
    for (size_t i = 0; i < schema.columnCount(); ++i) {
        values.push_back(Value::null());
    }
    return Row(std::move(values));
}
```

- [ ] **Step 4: Implement combineRows() helper**

```cpp
Row Executor::combineRows(const Row& left, const Row& right,
                          const Schema& left_schema, const Schema& right_schema) const {
    std::vector<Value> combined;
    combined.reserve(left_schema.columnCount() + right_schema.columnCount());
    
    // Add values from left row
    for (size_t i = 0; i < left_schema.columnCount(); ++i) {
        combined.push_back(left.get(i));
    }
    
    // Add values from right row
    for (size_t i = 0; i < right_schema.columnCount(); ++i) {
        combined.push_back(right.get(i));
    }
    
    return Row(std::move(combined));
}
```

- [ ] **Step 5: Implement buildJoinSchema() helper**

```cpp
Schema Executor::buildJoinSchema(const std::vector<Schema>& schemas,
                                  const std::vector<std::string>& table_aliases) const {
    std::vector<ColumnSchema> columns;
    
    for (size_t i = 0; i < schemas.size(); ++i) {
        const Schema& schema = schemas[i];
        const std::string& alias = table_aliases[i];
        
        for (size_t j = 0; j < schema.columnCount(); ++j) {
            const ColumnSchema& col = schema.column(j);
            
            // Check for duplicates across previous tables
            bool is_duplicate = false;
            for (size_t k = 0; k < i; ++k) {
                if (schemas[k].hasColumn(col.name())) {
                    is_duplicate = true;
                    break;
                }
            }
            
            // Prefix with table alias if duplicate
            std::string col_name = is_duplicate ? (alias + "." + col.name()) : col.name();
            columns.emplace_back(col_name, col.type(), col.nullable());
        }
    }
    
    return Schema(std::move(columns));
}
```

- [ ] **Step 6: Build and verify compilation**

Run: `cd /home/zxx/seeddb/build && make -j4`
Expected: Compilation succeeds

- [ ] **Step 7: Commit helper methods**

```bash
git add src/executor/executor.h src/executor/executor.cpp
git commit -m "feat(executor): add join helper methods

- makeNullRow(): create NULL-padded row for outer joins
- combineRows(): concatenate two rows
- buildJoinSchema(): build combined schema with duplicate column prefixing
- All methods ready for join execution"
```

### Task 6: Implement Cross Join Execution

**Files:**
- Modify: `src/executor/executor.cpp:324-475` (prepareSelect method)

- [ ] **Step 1: Write failing test for cross join**

```cpp
// In tests/unit/executor/test_executor.cpp
TEST_CASE("Execute cross join with two tables", "[executor][join]") {
    // Setup catalog with two tables
    Catalog catalog;
    
    // Create table a
    std::vector<ColumnSchema> cols_a;
    cols_a.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_a.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    Schema schema_a(std::move(cols_a));
    catalog.createTable("a", std::move(schema_a));
    Table* table_a = catalog.getTable("a");
    
    // Insert rows into a
    std::vector<Value> row_a1;
    row_a1.push_back(Value::integer(1));
    row_a1.push_back(Value::varchar("Alice"));
    table_a->insert(Row(std::move(row_a1)));
    
    std::vector<Value> row_a2;
    row_a2.push_back(Value::integer(2));
    row_a2.push_back(Value::varchar("Bob"));
    table_a->insert(Row(std::move(row_a2)));
    
    // Create table b
    std::vector<ColumnSchema> cols_b;
    cols_b.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_b.emplace_back("value", LogicalType(LogicalTypeId::INTEGER), true);
    Schema schema_b(std::move(cols_b));
    catalog.createTable("b", std::move(schema_b));
    Table* table_b = catalog.getTable("b");
    
    // Insert rows into b
    std::vector<Value> row_b1;
    row_b1.push_back(Value::integer(1));
    row_b1.push_back(Value::integer(100));
    table_b->insert(Row(std::move(row_b1)));
    
    std::vector<Value> row_b2;
    row_b2.push_back(Value::integer(2));
    row_b2.push_back(Value::integer(200));
    table_b->insert(Row(std::move(row_b2)));
    
    // Parse SELECT * FROM a, b
    parser::Lexer lexer("SELECT * FROM a, b");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    
    // Execute
    Executor executor(catalog);
    executor.execute(*stmt);
    
    // Verify 4 rows (2x2)
    size_t count = 0;
    while (executor.hasNext()) {
        executor.next();
        count++;
    }
    REQUIRE(count == 4);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: FAIL - prepareJoinSelect not implemented

- [ ] **Step 3: Implement executeCrossJoin() method**

```cpp
// In executor.cpp, after helper methods
void Executor::executeCrossJoin(const std::vector<Table*>& tables,
                                 const parser::SelectStmt& stmt) {
    if (tables.size() != 2) {
        // TODO: Support 3+ tables
        return;
    }
    
    Table* left_table = tables[0];
    Table* right_table = tables[1];
    
    const Schema& left_schema = left_table->schema();
    const Schema& right_schema = right_table->schema();
    
    // Build combined schema
    std::vector<Schema> schemas = {left_schema, right_schema};
    std::vector<std::string> aliases = {
        stmt.fromTable()->hasAlias() ? stmt.fromTable()->alias() : stmt.fromTable()->name(),
        stmt.joins()[0]->table()->hasAlias() ? stmt.joins()[0]->table()->alias() : stmt.joins()[0]->table()->name()
    };
    Schema join_schema = buildJoinSchema(schemas, aliases);
    
    // Apply WHERE clause filter if present
    const parser::Expr* where_clause = stmt.whereClause();
    
    // Cross product: nested loop
    for (size_t i = 0; i < left_table->rowCount(); ++i) {
        const Row& left_row = left_table->get(i);
        for (size_t j = 0; j < right_table->rowCount(); ++j) {
            const Row& right_row = right_table->get(j);
            
            // Combine rows
            Row combined = combineRows(left_row, right_row, left_schema, right_schema);
            
            // Apply WHERE filter
            if (!where_clause || evaluateWhereClause(where_clause, combined, join_schema)) {
                result_rows_.push_back(std::move(combined));
            }
        }
    }
}
```

- [ ] **Step 4: Implement prepareJoinSelect() method**

```cpp
bool Executor::prepareJoinSelect(const parser::SelectStmt& stmt) {
    // Collect all tables
    std::vector<Table*> tables;
    std::vector<Schema> schemas;
    
    // Add first table
    const parser::TableRef* first_table_ref = stmt.fromTable();
    if (!catalog_.hasTable(first_table_ref->name())) {
        return false;
    }
    tables.push_back(catalog_.getTable(first_table_ref->name()));
    schemas.push_back(tables.back()->schema());
    
    // Add joined tables
    for (const auto& join : stmt.joins()) {
        const std::string& table_name = join->table()->name();
        if (!catalog_.hasTable(table_name)) {
            return false;
        }
        tables.push_back(catalog_.getTable(table_name));
        schemas.push_back(tables.back()->schema());
    }
    
    // Execute based on join type
    // For now, all are CROSS JOIN (comma-separated)
    executeCrossJoin(tables, stmt);
    
    return true;
}
```

- [ ] **Step 5: Update prepareSelect() to handle joins**

```cpp
// In executor.cpp, modify prepareSelect() (around line 324)
bool Executor::prepareSelect(const parser::SelectStmt& stmt) {
    // Reset any previous query state
    resetQuery();

    // Get table name from FROM clause
    const parser::TableRef* table_ref = stmt.fromTable();
    if (!table_ref) {
        return false;
    }

    // NEW: Check if this is a join query
    if (stmt.hasJoins()) {
        return prepareJoinSelect(stmt);
    }

    // Single table logic (existing code continues...)
    const std::string& table_name = table_ref->name();
    
    // ... rest of existing prepareSelect code ...
```

- [ ] **Step 6: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: PASS

- [ ] **Step 7: Commit cross join execution**

```bash
git add src/executor/executor.h src/executor/executor.cpp tests/unit/executor/test_executor.cpp
git commit -m "feat(executor): implement cross join execution

- executeCrossJoin(): nested-loop cross product
- prepareJoinSelect(): route to appropriate join method
- Support WHERE clause filtering on join results
- Add test for 2-table cross join"
```

### Task 7: Verify End-to-End Comma-Separated Tables

**Files:**
- Modify: `tests/unit/executor/test_executor.cpp`

- [ ] **Step 1: Write integration test**

```cpp
TEST_CASE("End-to-end comma-separated tables with SELECT", "[executor][join][integration]") {
    Catalog catalog;
    
    // Create users table
    std::vector<ColumnSchema> user_cols;
    user_cols.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    user_cols.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("users", Schema(std::move(user_cols)));
    
    // Insert users
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::varchar("Alice"));
        catalog.getTable("users")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::varchar("Bob"));
        catalog.getTable("users")->insert(Row(std::move(row)));
    }
    
    // Create orders table
    std::vector<ColumnSchema> order_cols;
    order_cols.emplace_back("user_id", LogicalType(LogicalTypeId::INTEGER), true);
    order_cols.emplace_back("amount", LogicalType(LogicalTypeId::INTEGER), true);
    catalog.createTable("orders", Schema(std::move(order_cols)));
    
    // Insert orders
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(100));
        catalog.getTable("orders")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(200));
        catalog.getTable("orders")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::integer(150));
        catalog.getTable("orders")->insert(Row(std::move(row)));
    }
    
    // Parse and execute: SELECT * FROM users, orders
    parser::Lexer lexer("SELECT * FROM users, orders");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    
    Executor executor(catalog);
    executor.execute(*stmt);
    
    // Should get 6 rows (2 users x 3 orders)
    size_t count = 0;
    while (executor.hasNext()) {
        ExecutionResult row_result = executor.next();
        REQUIRE(row_result.hasRow());
        count++;
    }
    REQUIRE(count == 6);
}
```

- [ ] **Step 2: Run test**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/executor/test_executor "[executor][join][integration]" -v`
Expected: PASS

- [ ] **Step 3: Test with WHERE clause**

```cpp
TEST_CASE("Cross join with WHERE clause", "[executor][join]") {
    Catalog catalog;
    
    // Create and populate tables (same as previous test)
    std::vector<ColumnSchema> user_cols;
    user_cols.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    user_cols.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("users", Schema(std::move(user_cols)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::varchar("Alice"));
        catalog.getTable("users")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::varchar("Bob"));
        catalog.getTable("users")->insert(Row(std::move(row)));
    }
    
    std::vector<ColumnSchema> order_cols;
    order_cols.emplace_back("user_id", LogicalType(LogicalTypeId::INTEGER), true);
    order_cols.emplace_back("amount", LogicalType(LogicalTypeId::INTEGER), true);
    catalog.createTable("orders", Schema(std::move(order_cols)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(100));
        catalog.getTable("orders")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::integer(150));
        catalog.getTable("orders")->insert(Row(std::move(row)));
    }
    
    // SELECT * FROM users, orders WHERE users.id = orders.user_id
    parser::Lexer lexer("SELECT * FROM users, orders WHERE users.id = orders.user_id");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    
    Executor executor(catalog);
    executor.execute(*stmt);
    
    // Should get 2 matching rows
    size_t count = 0;
    while (executor.hasNext()) {
        executor.next();
        count++;
    }
    REQUIRE(count == 2);
}
```

- [ ] **Step 4: Run test**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: PASS

- [ ] **Step 5: Commit verification tests**

```bash
git add tests/unit/executor/test_executor.cpp
git commit -m "test(executor): add integration tests for comma-separated tables

- Test 2x3 cross product
- Test with WHERE clause filtering
- Verify cross join works end-to-end"
```

- [ ] **Step 6: Run all tests to ensure no regressions**

Run: `cd /home/zxx/seeddb/build && ctest --output-on-failure`
Expected: All tests PASS

---

## Phase 2.5b: Explicit CROSS JOIN Syntax

### Task 8: Parse Explicit CROSS JOIN

**Files:**
- Modify: `src/parser/parser.cpp:297-320` (parseSelect)

- [ ] **Step 1: Write failing test for explicit CROSS JOIN**

```cpp
TEST_CASE("Parse explicit CROSS JOIN", "[parser][join]") {
    parser::Lexer lexer("SELECT * FROM a CROSS JOIN b");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(stmt->fromTable()->name() == "a");
    REQUIRE(stmt->hasJoins());
    REQUIRE(stmt->joins().size() == 1);
    REQUIRE(stmt->joins()[0]->joinType() == parser::JoinType::CROSS);
    REQUIRE(stmt->joins()[0]->table()->name() == "b");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/parser/test_parser "[parser][join]" -v`
Expected: FAIL - parser doesn't recognize CROSS JOIN keyword

- [ ] **Step 3: Implement parseJoinClause() method**

```cpp
// In parser.cpp, add new method after parseTableRef()
Result<std::unique_ptr<JoinClause>> Parser::parseJoinClause() {
    // Determine join type
    JoinType join_type;
    
    if (match(TokenType::CROSS)) {
        // CROSS JOIN
        if (!match(TokenType::JOIN)) {
            return syntax_error<std::unique_ptr<JoinClause>>("Expected JOIN after CROSS");
        }
        join_type = JoinType::CROSS;
    } else if (match(TokenType::INNER)) {
        // INNER JOIN
        if (!match(TokenType::JOIN)) {
            return syntax_error<std::unique_ptr<JoinClause>>("Expected JOIN after INNER");
        }
        join_type = JoinType::INNER;
    } else if (match(TokenType::LEFT)) {
        // LEFT [OUTER] JOIN
        match(TokenType::OUTER);  // Optional OUTER keyword
        if (!match(TokenType::JOIN)) {
            return syntax_error<std::unique_ptr<JoinClause>>("Expected JOIN after LEFT");
        }
        join_type = JoinType::LEFT;
    } else if (match(TokenType::RIGHT)) {
        // RIGHT [OUTER] JOIN
        match(TokenType::OUTER);  // Optional OUTER keyword
        if (!match(TokenType::JOIN)) {
            return syntax_error<std::unique_ptr<JoinClause>>("Expected JOIN after RIGHT");
        }
        join_type = JoinType::RIGHT;
    } else if (match(TokenType::JOIN)) {
        // Plain JOIN (defaults to INNER)
        join_type = JoinType::INNER;
    } else {
        return syntax_error<std::unique_ptr<JoinClause>>("Expected JOIN type");
    }
    
    // Parse table reference
    auto table = parseTableRef();
    if (!table.is_ok()) {
        return Result<std::unique_ptr<JoinClause>>::err(table.error());
    }
    
    // Parse ON condition (optional for CROSS JOIN, required for others)
    std::unique_ptr<Expr> condition;
    if (join_type != JoinType::CROSS) {
        if (!match(TokenType::ON)) {
            return syntax_error<std::unique_ptr<JoinClause>>("Expected ON clause");
        }
        auto cond = parseExpression();
        if (!cond.is_ok()) {
            return Result<std::unique_ptr<JoinClause>>::err(cond.error());
        }
        condition = std::move(cond.value());
    }
    
    return Result<std::unique_ptr<JoinClause>>::ok(
        std::make_unique<JoinClause>(join_type, std::move(table.value()), std::move(condition))
    );
}
```

- [ ] **Step 4: Update parseSelect() to handle explicit JOINs**

```cpp
// In parser.cpp, in parseSelect(), after comma-separated tables parsing
    // ... existing comma parsing code ...
    
    // NEW: Parse explicit JOIN syntax
    while (check(TokenType::INNER) || check(TokenType::LEFT) || 
           check(TokenType::RIGHT) || check(TokenType::CROSS) || 
           check(TokenType::JOIN)) {
        auto join = parseJoinClause();
        if (!join.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(join.error());
        }
        stmt->addJoin(std::move(join.value()));
    }
    
    // Optional WHERE
    if (match(TokenType::WHERE)) {
```

- [ ] **Step 5: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/parser/test_parser "[parser][join]" -v`
Expected: PASS

- [ ] **Step 6: Commit explicit CROSS JOIN parsing**

```bash
git add src/parser/parser.cpp tests/unit/parser/test_parser.cpp
git commit -m "feat(parser): parse explicit CROSS JOIN syntax

- Implement parseJoinClause() for all join types
- Support CROSS JOIN keyword
- Parse table reference and optional ON clause
- Add test for explicit CROSS JOIN"
```

---

## Phase 2.5c: INNER JOIN

### Task 9: Implement INNER JOIN Execution

**Files:**
- Modify: `src/executor/executor.cpp` (add executeInnerJoin method)

- [ ] **Step 1: Write failing test for INNER JOIN**

```cpp
TEST_CASE("Execute INNER JOIN with ON condition", "[executor][join]") {
    Catalog catalog;
    
    // Create and populate table a
    std::vector<ColumnSchema> cols_a;
    cols_a.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_a.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("a", Schema(std::move(cols_a)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::varchar("Alice"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::varchar("Bob"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    
    // Create and populate table b
    std::vector<ColumnSchema> cols_b;
    cols_b.emplace_back("aid", LogicalType(LogicalTypeId::INTEGER), true);
    cols_b.emplace_back("value", LogicalType(LogicalTypeId::INTEGER), true);
    catalog.createTable("b", Schema(std::move(cols_b)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(100));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(200));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(3));  // No match in a
        row.push_back(Value::integer(300));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    
    // Parse: SELECT * FROM a INNER JOIN b ON a.id = b.aid
    parser::Lexer lexer("SELECT * FROM a INNER JOIN b ON a.id = b.aid");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    
    Executor executor(catalog);
    executor.execute(*stmt);
    
    // Should get 2 rows (Alice matches 2 b rows, Bob matches 0)
    size_t count = 0;
    while (executor.hasNext()) {
        executor.next();
        count++;
    }
    REQUIRE(count == 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: FAIL - INNER JOIN not executed

- [ ] **Step 3: Implement executeInnerJoin() method**

```cpp
// In executor.cpp, after executeCrossJoin()
void Executor::executeInnerJoin(Table* left_table, Table* right_table,
                                 const parser::Expr* condition,
                                 const parser::SelectStmt& stmt) {
    const Schema& left_schema = left_table->schema();
    const Schema& right_schema = right_table->schema();
    
    // Build combined schema
    std::vector<Schema> schemas = {left_schema, right_schema};
    std::vector<std::string> aliases = {
        stmt.fromTable()->hasAlias() ? stmt.fromTable()->alias() : stmt.fromTable()->name(),
        stmt.joins()[0]->table()->hasAlias() ? stmt.joins()[0]->table()->alias() : stmt.joins()[0]->table()->name()
    };
    Schema join_schema = buildJoinSchema(schemas, aliases);
    
    // Apply WHERE clause filter if present
    const parser::Expr* where_clause = stmt.whereClause();
    
    // Nested loop with ON condition filter
    for (size_t i = 0; i < left_table->rowCount(); ++i) {
        const Row& left_row = left_table->get(i);
        for (size_t j = 0; j < right_table->rowCount(); ++j) {
            const Row& right_row = right_table->get(j);
            
            // Combine rows for condition evaluation
            Row combined = combineRows(left_row, right_row, left_schema, right_schema);
            
            // Evaluate ON condition
            Value cond_result = evaluateExpr(condition, combined, join_schema);
            if (!cond_result.isNull() && cond_result.asBool()) {
                // Apply WHERE filter
                if (!where_clause || evaluateWhereClause(where_clause, combined, join_schema)) {
                    result_rows_.push_back(std::move(combined));
                }
            }
        }
    }
}
```

- [ ] **Step 4: Update prepareJoinSelect() to handle INNER JOIN**

```cpp
// In executor.cpp, modify prepareJoinSelect()
bool Executor::prepareJoinSelect(const parser::SelectStmt& stmt) {
    // ... existing code to collect tables ...
    
    // Execute based on join type
    const auto& first_join = stmt.joins()[0];
    
    switch (first_join->joinType()) {
        case parser::JoinType::CROSS:
            executeCrossJoin(tables, stmt);
            break;
        case parser::JoinType::INNER:
            executeInnerJoin(tables[0], tables[1], 
                           first_join->condition(), stmt);
            break;
        case parser::JoinType::LEFT:
            // TODO: Implement in Phase 2.5d
            break;
        case parser::JoinType::RIGHT:
            // TODO: Implement in Phase 2.5d
            break;
    }
    
    return true;
}
```

- [ ] **Step 5: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: PASS

- [ ] **Step 6: Commit INNER JOIN execution**

```bash
git add src/executor/executor.cpp tests/unit/executor/test_executor.cpp
git commit -m "feat(executor): implement INNER JOIN execution

- executeInnerJoin(): nested loop with ON condition filter
- Route join execution based on JoinType
- Support qualified column references in ON clause
- Add test for INNER JOIN with ON condition"
```

---

## Phase 2.5d: LEFT/RIGHT JOIN

### Task 10: Implement LEFT JOIN

**Files:**
- Modify: `src/executor/executor.cpp` (add executeLeftJoin method)

- [ ] **Step 1: Write failing test for LEFT JOIN**

```cpp
TEST_CASE("Execute LEFT JOIN with NULL padding", "[executor][join]") {
    Catalog catalog;
    
    // Create table a with 3 rows
    std::vector<ColumnSchema> cols_a;
    cols_a.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_a.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("a", Schema(std::move(cols_a)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::varchar("Alice"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::varchar("Bob"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(3));
        row.push_back(Value::varchar("Charlie"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    
    // Create table b with only 2 matching rows
    std::vector<ColumnSchema> cols_b;
    cols_b.emplace_back("aid", LogicalType(LogicalTypeId::INTEGER), true);
    cols_b.emplace_back("value", LogicalType(LogicalTypeId::INTEGER), true);
    catalog.createTable("b", Schema(std::move(cols_b)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(100));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::integer(200));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    
    // Parse: SELECT * FROM a LEFT JOIN b ON a.id = b.aid
    parser::Lexer lexer("SELECT * FROM a LEFT JOIN b ON a.id = b.aid");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    auto* stmt = static_cast<parser::SelectStmt*>(result.value().get());
    
    Executor executor(catalog);
    executor.execute(*stmt);
    
    // Should get 3 rows (Charlie has NULL for b.*)
    size_t count = 0;
    bool charlie_has_null = false;
    while (executor.hasNext()) {
        ExecutionResult row_result = executor.next();
        const Row& row = row_result.row();
        
        // Check if this is Charlie's row (id=3)
        if (row.get(0).asInt() == 3) {
            // b.aid should be NULL
            REQUIRE(row.get(2).isNull());
            // b.value should be NULL
            REQUIRE(row.get(3).isNull());
            charlie_has_null = true;
        }
        count++;
    }
    REQUIRE(count == 3);
    REQUIRE(charlie_has_null);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /home/zxx/seeddb/build && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: FAIL - LEFT JOIN not implemented

- [ ] **Step 3: Implement executeLeftJoin() method**

```cpp
// In executor.cpp, after executeInnerJoin()
void Executor::executeLeftJoin(Table* left_table, Table* right_table,
                                const parser::Expr* condition,
                                const parser::SelectStmt& stmt) {
    const Schema& left_schema = left_table->schema();
    const Schema& right_schema = right_table->schema();
    
    // Build combined schema
    std::vector<Schema> schemas = {left_schema, right_schema};
    std::vector<std::string> aliases = {
        stmt.fromTable()->hasAlias() ? stmt.fromTable()->alias() : stmt.fromTable()->name(),
        stmt.joins()[0]->table()->hasAlias() ? stmt.joins()[0]->table()->alias() : stmt.joins()[0]->table()->name()
    };
    Schema join_schema = buildJoinSchema(schemas, aliases);
    
    // Apply WHERE clause filter if present
    const parser::Expr* where_clause = stmt.whereClause();
    
    // Nested loop with NULL padding
    for (size_t i = 0; i < left_table->rowCount(); ++i) {
        const Row& left_row = left_table->get(i);
        bool matched = false;
        
        for (size_t j = 0; j < right_table->rowCount(); ++j) {
            const Row& right_row = right_table->get(j);
            
            // Combine rows for condition evaluation
            Row combined = combineRows(left_row, right_row, left_schema, right_schema);
            
            // Evaluate ON condition
            Value cond_result = evaluateExpr(condition, combined, join_schema);
            if (!cond_result.isNull() && cond_result.asBool()) {
                matched = true;
                // Apply WHERE filter
                if (!where_clause || evaluateWhereClause(where_clause, combined, join_schema)) {
                    result_rows_.push_back(std::move(combined));
                }
            }
        }
        
        // No match - emit NULL-padded row
        if (!matched) {
            Row null_right = makeNullRow(right_schema);
            Row combined = combineRows(left_row, null_right, left_schema, right_schema);
            // Apply WHERE filter
            if (!where_clause || evaluateWhereClause(where_clause, combined, join_schema)) {
                result_rows_.push_back(std::move(combined));
            }
        }
    }
}
```

- [ ] **Step 4: Update prepareJoinSelect() to handle LEFT JOIN**

```cpp
// In prepareJoinSelect(), add case for LEFT JOIN
        case parser::JoinType::LEFT:
            executeLeftJoin(tables[0], tables[1],
                          first_join->condition(), stmt);
            break;
```

- [ ] **Step 5: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: PASS

- [ ] **Step 6: Commit LEFT JOIN execution**

```bash
git add src/executor/executor.cpp tests/unit/executor/test_executor.cpp
git commit -m "feat(executor): implement LEFT JOIN with NULL padding

- executeLeftJoin(): pad non-matching rows with NULLs
- Preserve all left table rows even if no match
- Verify NULL values in unmatched rows
- Add test for LEFT JOIN NULL handling"
```

### Task 11: Implement RIGHT JOIN

**Files:**
- Modify: `src/executor/executor.cpp` (add executeRightJoin method)

- [ ] **Step 1: Write test for RIGHT JOIN**

```cpp
TEST_CASE("Execute RIGHT JOIN", "[executor][join]") {
    Catalog catalog;
    
    // Create table a with 2 rows
    std::vector<ColumnSchema> cols_a;
    cols_a.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_a.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("a", Schema(std::move(cols_a)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::varchar("Alice"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    
    // Create table b with 3 rows (2 matching, 1 non-matching)
    std::vector<ColumnSchema> cols_b;
    cols_b.emplace_back("aid", LogicalType(LogicalTypeId::INTEGER), true);
    cols_b.emplace_back("value", LogicalType(LogicalTypeId::INTEGER), true);
    catalog.createTable("b", Schema(std::move(cols_b)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(100));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(2));
        row.push_back(Value::integer(200));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    {
        std::vector<Value> row;
        row.push_back(Value::integer(3));
        row.push_back(Value::integer(300));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    
    // Parse: SELECT * FROM a RIGHT JOIN b ON a.id = b.aid
    parser::Lexer lexer("SELECT * FROM a RIGHT JOIN b ON a.id = b.aid");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    Executor executor(catalog);
    executor.execute(*result.value().get()->asSelect());
    
    // Should get 3 rows (all b rows preserved)
    size_t count = 0;
    while (executor.hasNext()) {
        executor.next();
        count++;
    }
    REQUIRE(count == 3);
}
```

- [ ] **Step 2: Implement executeRightJoin() as reverse LEFT JOIN**

```cpp
// In executor.cpp
void Executor::executeRightJoin(Table* left_table, Table* right_table,
                                 const parser::Expr* condition,
                                 const parser::SelectStmt& stmt) {
    // RIGHT JOIN is LEFT JOIN with tables reversed
    // Swap left and right tables, then swap result column order
    
    const Schema& left_schema = left_table->schema();
    const Schema& right_schema = right_table->schema();
    
    // Build combined schema (right first, then left for RIGHT JOIN)
    std::vector<Schema> schemas = {left_schema, right_schema};
    std::vector<std::string> aliases = {
        stmt.fromTable()->hasAlias() ? stmt.fromTable()->alias() : stmt.fromTable()->name(),
        stmt.joins()[0]->table()->hasAlias() ? stmt.joins()[0]->table()->alias() : stmt.joins()[0]->table()->name()
    };
    Schema join_schema = buildJoinSchema(schemas, aliases);
    
    const parser::Expr* where_clause = stmt.whereClause();
    
    // Reverse LEFT JOIN logic: preserve all RIGHT rows
    for (size_t j = 0; j < right_table->rowCount(); ++j) {
        const Row& right_row = right_table->get(j);
        bool matched = false;
        
        for (size_t i = 0; i < left_table->rowCount(); ++i) {
            const Row& left_row = left_table->get(i);
            
            Row combined = combineRows(left_row, right_row, left_schema, right_schema);
            
            Value cond_result = evaluateExpr(condition, combined, join_schema);
            if (!cond_result.isNull() && cond_result.asBool()) {
                matched = true;
                if (!where_clause || evaluateWhereClause(where_clause, combined, join_schema)) {
                    result_rows_.push_back(std::move(combined));
                }
            }
        }
        
        // No match - emit NULL-padded row (NULL for left, values for right)
        if (!matched) {
            Row null_left = makeNullRow(left_schema);
            Row combined = combineRows(null_left, right_row, left_schema, right_schema);
            if (!where_clause || evaluateWhereClause(where_clause, combined, join_schema)) {
                result_rows_.push_back(std::move(combined));
            }
        }
    }
}
```

- [ ] **Step 3: Update prepareJoinSelect() to handle RIGHT JOIN**

```cpp
        case parser::JoinType::RIGHT:
            executeRightJoin(tables[0], tables[1],
                           first_join->condition(), stmt);
            break;
```

- [ ] **Step 4: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: PASS

- [ ] **Step 5: Commit RIGHT JOIN execution**

```bash
git add src/executor/executor.cpp tests/unit/executor/test_executor.cpp
git commit -m "feat(executor): implement RIGHT JOIN execution

- executeRightJoin(): reverse of LEFT JOIN
- Preserve all right table rows
- Pad non-matching rows with NULLs
- Add test for RIGHT JOIN"
```

---

## Phase 2.5e: Multi-Table Joins

### Task 12: Support Three-Table Joins

**Files:**
- Modify: `src/executor/executor.cpp` (extend join execution)

- [ ] **Step 1: Write test for three-table join**

```cpp
TEST_CASE("Execute three-table join", "[executor][join]") {
    Catalog catalog;
    
    // Create table a
    std::vector<ColumnSchema> cols_a;
    cols_a.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_a.emplace_back("name", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("a", Schema(std::move(cols_a)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::varchar("Alice"));
        catalog.getTable("a")->insert(Row(std::move(row)));
    }
    
    // Create table b
    std::vector<ColumnSchema> cols_b;
    cols_b.emplace_back("aid", LogicalType(LogicalTypeId::INTEGER), true);
    cols_b.emplace_back("cid", LogicalType(LogicalTypeId::INTEGER), true);
    catalog.createTable("b", Schema(std::move(cols_b)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(1));
        row.push_back(Value::integer(10));
        catalog.getTable("b")->insert(Row(std::move(row)));
    }
    
    // Create table c
    std::vector<ColumnSchema> cols_c;
    cols_c.emplace_back("id", LogicalType(LogicalTypeId::INTEGER), true);
    cols_c.emplace_back("desc", LogicalType(LogicalTypeId::VARCHAR), true);
    catalog.createTable("c", Schema(std::move(cols_c)));
    {
        std::vector<Value> row;
        row.push_back(Value::integer(10));
        row.push_back(Value::varchar("Desc10"));
        catalog.getTable("c")->insert(Row(std::move(row)));
    }
    
    // Parse: SELECT * FROM a JOIN b ON a.id = b.aid JOIN c ON b.cid = c.id
    parser::Lexer lexer("SELECT * FROM a INNER JOIN b ON a.id = b.aid INNER JOIN c ON b.cid = c.id");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    Executor executor(catalog);
    executor.execute(*result.value().get()->asSelect());
    
    // Should get 1 row (all three tables match)
    size_t count = 0;
    while (executor.hasNext()) {
        ExecutionResult row_result = executor.next();
        const Row& row = row_result.row();
        REQUIRE(row.size() == 6);  // 2 + 2 + 2 columns
        count++;
    }
    REQUIRE(count == 1);
}
```

- [ ] **Step 2: Implement chained join execution**

```cpp
// In executor.cpp, modify prepareJoinSelect() to handle multiple joins
bool Executor::prepareJoinSelect(const parser::SelectStmt& stmt) {
    // Collect all tables
    std::vector<Table*> tables;
    std::vector<Schema> schemas;
    std::vector<std::string> aliases;
    
    // Add first table
    const parser::TableRef* first_table_ref = stmt.fromTable();
    if (!catalog_.hasTable(first_table_ref->name())) {
        return false;
    }
    tables.push_back(catalog_.getTable(first_table_ref->name()));
    schemas.push_back(tables.back()->schema());
    aliases.push_back(first_table_ref->hasAlias() ? first_table_ref->alias() : first_table_ref->name());
    
    // Process joins sequentially
    for (const auto& join : stmt.joins()) {
        const std::string& table_name = join->table()->name();
        if (!catalog_.hasTable(table_name)) {
            return false;
        }
        
        Table* right_table = catalog_.getTable(table_name);
        const Schema& right_schema = right_table->schema();
        std::string right_alias = join->table()->hasAlias() ? 
                                  join->table()->alias() : table_name;
        
        // Execute join with accumulated result
        executeBinaryJoin(tables, schemas, aliases, 
                         right_table, right_schema, right_alias,
                         join.get(), stmt);
        
        // Update accumulated tables/schemas
        tables.push_back(right_table);
        schemas.clear();
        schemas.push_back(buildJoinSchema(
            std::vector<Schema>{schemas.begin(), schemas.end()}, 
            aliases
        ));
        aliases.push_back(right_alias);
    }
    
    return true;
}

void Executor::executeBinaryJoin(
    const std::vector<Table*>& left_tables,
    const std::vector<Schema>& left_schemas,
    const std::vector<std::string>& left_aliases,
    Table* right_table,
    const Schema& right_schema,
    const std::string& right_alias,
    const parser::JoinClause* join,
    const parser::SelectStmt& stmt
) {
    // Build left combined schema
    Schema left_combined = buildJoinSchema(left_schemas, left_aliases);
    
    // Build join schema
    std::vector<Schema> join_schemas;
    join_schemas.push_back(left_combined);
    join_schemas.push_back(right_schema);
    
    std::vector<std::string> join_aliases;
    join_aliases.push_back("left");  // Combined left
    join_aliases.push_back(right_alias);
    
    Schema join_schema = buildJoinSchema(join_schemas, join_aliases);
    
    // Get left rows (from result_rows_ if we've done previous joins, or from first table)
    std::vector<Row> left_rows;
    if (result_rows_.empty()) {
        // First join: use first table's rows
        for (size_t i = 0; i < left_tables[0]->rowCount(); ++i) {
            left_rows.push_back(left_tables[0]->get(i));
        }
    } else {
        // Subsequent join: use result from previous join
        left_rows = std::move(result_rows_);
        result_rows_.clear();
    }
    
    // Execute join based on type
    switch (join->joinType()) {
        case parser::JoinType::CROSS:
            for (const auto& left_row : left_rows) {
                for (size_t j = 0; j < right_table->rowCount(); ++j) {
                    const Row& right_row = right_table->get(j);
                    Row combined = combineRows(left_row, right_row, 
                                              left_combined, right_schema);
                    result_rows_.push_back(std::move(combined));
                }
            }
            break;
            
        case parser::JoinType::INNER:
            for (const auto& left_row : left_rows) {
                for (size_t j = 0; j < right_table->rowCount(); ++j) {
                    const Row& right_row = right_table->get(j);
                    Row combined = combineRows(left_row, right_row,
                                              left_combined, right_schema);
                    
                    Value cond_result = evaluateExpr(join->condition(), combined, join_schema);
                    if (!cond_result.isNull() && cond_result.asBool()) {
                        result_rows_.push_back(std::move(combined));
                    }
                }
            }
            break;
            
        case parser::JoinType::LEFT:
            for (const auto& left_row : left_rows) {
                bool matched = false;
                for (size_t j = 0; j < right_table->rowCount(); ++j) {
                    const Row& right_row = right_table->get(j);
                    Row combined = combineRows(left_row, right_row,
                                              left_combined, right_schema);
                    
                    Value cond_result = evaluateExpr(join->condition(), combined, join_schema);
                    if (!cond_result.isNull() && cond_result.asBool()) {
                        matched = true;
                        result_rows_.push_back(std::move(combined));
                    }
                }
                
                if (!matched) {
                    Row null_right = makeNullRow(right_schema);
                    Row combined = combineRows(left_row, null_right,
                                              left_combined, right_schema);
                    result_rows_.push_back(std::move(combined));
                }
            }
            break;
            
        case parser::JoinType::RIGHT:
            // Similar to LEFT but reversed
            // TODO: Implement if needed
            break;
    }
}
```

- [ ] **Step 3: Build and run test**

Run: `cd /home/zxx/seeddb/build && make -j4 && ./tests/unit/executor/test_executor "[executor][join]" -v`
Expected: PASS

- [ ] **Step 4: Commit multi-table join support**

```bash
git add src/executor/executor.cpp tests/unit/executor/test_executor.cpp
git commit -m "feat(executor): support multi-table joins (3+ tables)

- Chain binary joins sequentially
- Accumulate result rows between joins
- Support INNER JOIN chains
- Add test for three-table join"
```

---

## Final Verification

### Task 13: Run All Tests

- [ ] **Step 1: Run all unit tests**

Run: `cd /home/zxx/seeddb/build && ctest --output-on-failure`
Expected: All tests PASS

- [ ] **Step 2: Test final milestone query in CLI**

Run: `cd /home/zxx/seeddb/build && ./bin/seeddb_cli`

Execute:
```sql
CREATE TABLE users (id INT, name VARCHAR(50), status VARCHAR(20));
CREATE TABLE orders (id INT, user_id INT, amount INT);

INSERT INTO users VALUES (1, 'Alice', 'active');
INSERT INTO users VALUES (2, 'Bob', 'premium');
INSERT INTO users VALUES (3, 'Charlie', 'inactive');

INSERT INTO orders VALUES (1, 1, 500);
INSERT INTO orders VALUES (2, 1, 600);
INSERT INTO orders VALUES (3, 2, 200);

SELECT 
    u.name,
    COUNT(o.id) AS order_count,
    COALESCE(SUM(o.amount), 0) AS total_amount,
    CASE WHEN SUM(o.amount) > 1000 THEN 'VIP' ELSE 'Normal' END AS level
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
WHERE u.status IN ('active', 'premium')
GROUP BY u.id, u.name
HAVING COUNT(o.id) > 0
ORDER BY total_amount DESC
LIMIT 10;
```

Expected: Returns correct results with joins, aggregates, and filters

- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "feat: complete Phase 2.5 JOIN support

- CROSS JOIN (implicit and explicit)
- INNER JOIN with ON clause
- LEFT JOIN with NULL padding
- RIGHT JOIN with NULL padding
- Multi-table joins (3+ tables)
- Duplicate column prefixing (PostgreSQL-style)
- Integration with WHERE, GROUP BY, HAVING, ORDER BY, LIMIT

All tests passing. Ready for Phase 3."
```

---

## Success Criteria Checklist

- [ ] F2-21: `SELECT * FROM a, b` returns cross product (2x2 = 4 rows)
- [ ] F2-21: `SELECT * FROM a CROSS JOIN b` returns cross product
- [ ] F2-22: `SELECT * FROM a INNER JOIN b ON a.id = b.id` filters by condition
- [ ] F2-23: `SELECT * FROM a LEFT JOIN b ON a.id = b.id` pads with NULLs for unmatched rows
- [ ] F2-24: `SELECT * FROM a RIGHT JOIN b ON a.id = b.id` preserves all right rows
- [ ] F2-25: `SELECT * FROM a JOIN b ON ... JOIN c ON ...` chains joins correctly
- [ ] Duplicate columns: `SELECT *` from tables with same column names shows prefixed names
- [ ] Error handling: Ambiguous column references detected
- [ ] Integration: JOIN works with WHERE, GROUP BY, HAVING, ORDER BY, LIMIT
- [ ] All unit tests pass (`ctest`)
- [ ] Final milestone query executes correctly in CLI

---

## References

- Design Spec: `docs/superpowers/specs/2026-03-23-join-support-design.md`
- PostgreSQL JOIN Documentation: https://www.postgresql.org/docs/current/queries-table-expressions.html
- Existing Executor Patterns: `src/executor/executor.cpp`
- Test Patterns: `tests/unit/parser/test_parser.cpp`, `tests/unit/executor/test_executor.cpp`
