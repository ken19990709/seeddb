# JOIN Support Implementation Design

**Date**: 2026-03-23  
**Phase**: 2.5 (基础 JOIN)  
**Status**: Design Complete  

## Overview

This document describes the implementation design for adding JOIN support to SeedDB, enabling multi-table queries with CROSS, INNER, LEFT, and RIGHT joins.

## Design Decisions

### 1. Join Algorithm: Simple Nested-Loop Join

**Rationale**: 
- Educational database focused on clarity, not performance
- Simple to implement and understand
- Appropriate for in-memory storage with small datasets
- Can be optimized later with hash join when needed

**Performance**: O(n×m) for two tables - acceptable for current scope

### 2. FROM Clause Strategy: Implicit First, Explicit Later

**Rationale**:
- Start with comma-separated tables (implicit cross join)
- Add explicit JOIN syntax incrementally
- Lower initial implementation risk
- Aligns with spec task breakdown (F2-21 to F2-25)

### 3. Duplicate Column Handling: PostgreSQL-Style

**Rationale**:
- Follow PostgreSQL/DuckDB standard behavior
- When SELECT * has duplicate column names, prefix with table name/alias
- Columns appear in order tables are listed in FROM clause
- Clean, predictable behavior for users

## Architecture

### AST Changes

#### New Types

```cpp
// In ast.h

enum class NodeType {
    // ... existing types ...
    JOIN_CLAUSE,  // NEW
};

enum class JoinType {
    CROSS,      // CROSS JOIN or comma-separated
    INNER,      // INNER JOIN ... ON
    LEFT,       // LEFT [OUTER] JOIN ... ON
    RIGHT       // RIGHT [OUTER] JOIN ... ON
};
```

#### New AST Node: JoinClause

```cpp
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
    
private:
    JoinType join_type_;
    std::unique_ptr<TableRef> table_;
    std::unique_ptr<Expr> condition_;  // nullptr for CROSS JOIN
};
```

#### Modified SelectStmt

```cpp
class SelectStmt : public Stmt {
    // ... existing members ...
    
    std::unique_ptr<TableRef> from_table_;              // First table (backward compatible)
    std::vector<std::unique_ptr<JoinClause>> joins_;     // Additional joins
    
public:
    const auto& joins() const { return joins_; }
    bool hasJoins() const { return !joins_.empty(); }
    void addJoin(std::unique_ptr<JoinClause> join) { 
        joins_.push_back(std::move(join)); 
    }
};
```

**Key Design Points**:
- `from_table_` remains first table (backward compatible)
- `joins_` contains additional tables with join type
- `JoinClause` encapsulates: table + join type + optional ON condition
- CROSS JOIN has `condition_ = nullptr`

### Parser Changes

#### New Methods

```cpp
// In parser.h

class Parser {
    // ... existing methods ...
    
    // NEW: JOIN parsing
    Result<std::unique_ptr<JoinClause>> parseJoinClause();
    Result<std::unique_ptr<Expr>> parseJoinCondition();
};
```

#### Parsing Logic

**Comma-separated tables**:
```cpp
// In parseSelect(), after parsing first table
while (match(TokenType::COMMA)) {
    auto table = parseTableRef();
    if (!table.is_ok()) {
        return error;
    }
    // Create CROSS JOIN clause
    stmt->addJoin(std::make_unique<JoinClause>(
        JoinType::CROSS, 
        std::move(table.value())
    ));
}
```

**Explicit JOIN syntax**:
```cpp
// After comma parsing
while (check(TokenType::INNER) || check(TokenType::LEFT) || 
       check(TokenType::RIGHT) || check(TokenType::CROSS)) {
    auto join = parseJoinClause();
    if (!join.is_ok()) {
        return error;
    }
    stmt->addJoin(std::move(join.value()));
}
```

### Executor Changes

#### Execution Strategy

```cpp
bool Executor::prepareSelect(const SelectStmt& stmt) {
    // Reset query state
    resetQuery();
    
    // Case 1: Single table (existing logic)
    if (!stmt.hasJoins()) {
        return prepareSingleTableSelect(stmt);
    }
    
    // Case 2: Multi-table join
    return prepareJoinSelect(stmt);
}
```

#### Join Execution Methods

```cpp
// New methods in executor.h

class Executor {
    // ... existing methods ...
    
    // NEW: Join execution
    bool prepareJoinSelect(const parser::SelectStmt& stmt);
    void executeCrossJoin(const std::vector<Table*>& tables, 
                          const parser::SelectStmt& stmt);
    void executeInnerJoin(const std::vector<Table*>& tables, 
                          const parser::Expr* condition,
                          const parser::SelectStmt& stmt);
    void executeLeftJoin(const std::vector<Table*>& tables, 
                         const parser::Expr* condition,
                         const parser::SelectStmt& stmt);
    void executeRightJoin(const std::vector<Table*>& tables, 
                          const parser::Expr* condition,
                          const parser::SelectStmt& stmt);
    
    // NEW: Join helpers
    Row combineRows(const Row& left, const Row& right, 
                    const Schema& left_schema, const Schema& right_schema);
    Row makeNullRow(const Schema& schema);
    Schema buildJoinSchema(const std::vector<Schema>& schemas,
                           const std::vector<std::string>& table_aliases);
    bool evaluateJoinCondition(const parser::Expr* condition,
                               const Row& left, const Row& right,
                               const Schema& left_schema, const Schema& right_schema);
};
```

#### Cross Join Implementation

```cpp
void Executor::executeCrossJoin(const std::vector<Table*>& tables,
                                 const SelectStmt& stmt) {
    // Build combined schema
    std::vector<Schema> schemas;
    for (const auto* table : tables) {
        schemas.push_back(table->schema());
    }
    Schema join_schema = buildJoinSchema(schemas, getTableAliases(stmt));
    
    // Nested loop: cross product
    if (tables.size() == 2) {
        for (size_t i = 0; i < tables[0]->rowCount(); ++i) {
            const Row& row_a = tables[0]->get(i);
            for (size_t j = 0; j < tables[1]->rowCount(); ++j) {
                const Row& row_b = tables[1]->get(j);
                Row combined = combineRows(row_a, row_b, 
                                          schemas[0], schemas[1]);
                result_rows_.push_back(std::move(combined));
            }
        }
    }
    // Handle 3+ tables recursively
}
```

#### Inner Join Implementation

```cpp
void Executor::executeInnerJoin(const std::vector<Table*>& tables,
                                 const Expr* condition,
                                 const SelectStmt& stmt) {
    // Build combined schema
    std::vector<Schema> schemas;
    for (const auto* table : tables) {
        schemas.push_back(table->schema());
    }
    Schema join_schema = buildJoinSchema(schemas, getTableAliases(stmt));
    
    // Nested loop with condition filter
    for (size_t i = 0; i < tables[0]->rowCount(); ++i) {
        const Row& row_a = tables[0]->get(i);
        for (size_t j = 0; j < tables[1]->rowCount(); ++j) {
            const Row& row_b = tables[1]->get(j);
            
            // Evaluate ON condition
            if (evaluateJoinCondition(condition, row_a, row_b, 
                                     schemas[0], schemas[1])) {
                Row combined = combineRows(row_a, row_b, 
                                          schemas[0], schemas[1]);
                result_rows_.push_back(std::move(combined));
            }
        }
    }
}
```

#### Left Join Implementation

```cpp
void Executor::executeLeftJoin(const std::vector<Table*>& tables,
                                const Expr* condition,
                                const SelectStmt& stmt) {
    // Build combined schema
    std::vector<Schema> schemas;
    for (const auto* table : tables) {
        schemas.push_back(table->schema());
    }
    Schema join_schema = buildJoinSchema(schemas, getTableAliases(stmt));
    
    // Nested loop with NULL padding
    for (size_t i = 0; i < tables[0]->rowCount(); ++i) {
        const Row& row_a = tables[0]->get(i);
        bool matched = false;
        
        for (size_t j = 0; j < tables[1]->rowCount(); ++j) {
            const Row& row_b = tables[1]->get(j);
            
            if (evaluateJoinCondition(condition, row_a, row_b,
                                     schemas[0], schemas[1])) {
                matched = true;
                Row combined = combineRows(row_a, row_b,
                                          schemas[0], schemas[1]);
                result_rows_.push_back(std::move(combined));
            }
        }
        
        // No match - emit NULL-padded row
        if (!matched) {
            Row null_row = makeNullRow(schemas[1]);
            Row combined = combineRows(row_a, null_row,
                                      schemas[0], schemas[1]);
            result_rows_.push_back(std::move(combined));
        }
    }
}
```

#### Column Resolution

**Qualified vs Unqualified References**:

```cpp
Value Executor::evaluateJoinCondition(const Expr* expr,
                                       const Row& left, const Row& right,
                                       const Schema& left_schema, 
                                       const Schema& right_schema) {
    if (expr->type() == NodeType::EXPR_COLUMN_REF) {
        const auto* col_ref = static_cast<const ColumnRef*>(expr);
        
        if (col_ref->hasTableQualifier()) {
            // Qualified: table.column
            const std::string& table = col_ref->table();
            const std::string& column = col_ref->column();
            
            if (table == left_table_alias) {
                return getColumnFromRow(left, left_schema, column);
            } else if (table == right_table_alias) {
                return getColumnFromRow(right, right_schema, column);
            }
        } else {
            // Unqualified: search left then right
            // (error if ambiguous - caught at parse time)
            return getColumnFromRow(left, left_schema, col_ref->column());
        }
    }
    // ... handle other expression types
}
```

#### Schema Building with Duplicate Handling

```cpp
Schema Executor::buildJoinSchema(const std::vector<Schema>& schemas,
                                  const std::vector<std::string>& aliases) {
    std::vector<ColumnSchema> columns;
    
    for (size_t i = 0; i < schemas.size(); ++i) {
        const Schema& schema = schemas[i];
        const std::string& alias = aliases[i];
        
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

## Integration Points

### Interaction with Existing Features

**With WHERE clause**: Applied after join
```sql
SELECT * FROM a, b WHERE a.id = b.aid AND a.status = 'active'
-- Execution: Cross join → Filter by entire WHERE
```

**With GROUP BY/HAVING**: Applied after join
```sql
SELECT a.name, COUNT(*) FROM a, b WHERE a.id = b.aid GROUP BY a.name
-- Execution: Join → Filter → Group → Aggregate
```

**With ORDER BY/LIMIT**: Applied after join (existing logic unchanged)
```sql
SELECT * FROM a, b ORDER BY a.id LIMIT 10
-- Execution: Join → Sort → Limit
```

### Backward Compatibility

Single-table queries work unchanged:
```cpp
if (!stmt.hasJoins()) {
    // Use existing single-table logic
    return prepareSingleTableSelect(stmt);
}
// New multi-table logic
return prepareJoinSelect(stmt);
```

## Error Handling

### Parse-Time Errors

**Ambiguous column reference** (in ON or WHERE clauses):
```cpp
// In parser.cpp
if (columnExistsInMultipleTables(col_name, tables)) {
    return error("Ambiguous column reference: '" + col_name + 
                "' appears in multiple tables. Use table.column format.");
}
```

**Invalid table reference**:
```cpp
if (!catalog_.hasTable(table_name)) {
    return error("Table '" + table_name + "' not found");
}
```

### Execute-Time Errors

- Invalid ON conditions evaluate to false/NULL (silent NULL behavior)
- Missing tables caught at prepare time
- Type mismatches in comparisons handled by existing logic

## Rollout Plan

### Phase 2.5a: Comma-Separated Tables (3-4 days)

**Tasks**:
- **F2-21: CROSS JOIN (implicit)**
  - Parser: Accept comma-separated tables
  - AST: Add `joins_` vector
  - Executor: Cross product with nested loops
  - Tests: Parser, executor, integration

**Deliverable**: `SELECT * FROM a, b` works

**Verification**:
```sql
CREATE TABLE a (id INT, name VARCHAR(50));
CREATE TABLE b (id INT, value INT);
INSERT INTO a VALUES (1, 'Alice'), (2, 'Bob');
INSERT INTO b VALUES (1, 100), (2, 200);
SELECT * FROM a, b;  -- Returns 4 rows (2x2)
```

### Phase 2.5b: Explicit CROSS JOIN (1 day)

**Tasks**:
- **F2-21: CROSS JOIN (explicit)**
  - Parser: Parse `CROSS JOIN` syntax
  - AST: `JoinClause` with `JoinType::CROSS`
  - Executor: Reuse cross product logic
  - Tests: Parser variations

**Deliverable**: `SELECT * FROM a CROSS JOIN b` works

### Phase 2.5c: INNER JOIN (1.5 days)

**Tasks**:
- **F2-22: INNER JOIN**
  - Parser: Parse `INNER JOIN ... ON ...`
  - Executor: Condition evaluation
  - Tests: ON clause, complex conditions

**Deliverable**: `SELECT * FROM a INNER JOIN b ON a.id = b.id` works

**Verification**:
```sql
SELECT * FROM a INNER JOIN b ON a.id = b.id;
-- Returns 2 rows (matching IDs)
```

### Phase 2.5d: LEFT/RIGHT JOIN (1.5 days)

**Tasks**:
- **F2-23: LEFT JOIN**
  - Executor: NULL padding for non-matching rows
  - Tests: NULL handling
  
- **F2-24: RIGHT JOIN**
  - Executor: Reverse of LEFT JOIN
  - Tests: NULL handling

**Deliverable**: Outer joins work with NULL padding

**Verification**:
```sql
INSERT INTO a VALUES (3, 'Charlie');  -- No matching b row
SELECT * FROM a LEFT JOIN b ON a.id = b.id;
-- Returns 3 rows, Charlie has NULL for b.*
```

### Phase 2.5e: Multi-Table Joins (1-2 days)

**Tasks**:
- **F2-25: Multi-table JOIN**
  - Executor: Chain joins (a JOIN b JOIN c)
  - Tests: 3+ tables, complex conditions

**Deliverable**: `SELECT * FROM a JOIN b ON ... JOIN c ON ...` works

**Verification**:
```sql
CREATE TABLE c (id INT, description VARCHAR(50));
INSERT INTO c VALUES (1, 'First'), (2, 'Second');
SELECT * FROM a 
  INNER JOIN b ON a.id = b.id 
  INNER JOIN c ON b.id = c.id;
-- Returns 2 rows
```

## Files to Modify

### Parser
- `src/parser/ast.h` - Add `JoinClause`, `JoinType` enum
- `src/parser/ast.cpp` - toString() implementations
- `src/parser/parser.h` - New parsing method declarations
- `src/parser/parser.cpp` - JOIN parsing logic

### Executor
- `src/executor/executor.h` - New join execution methods
- `src/executor/executor.cpp` - Join implementation

### Tests
- `tests/unit/parser/test_parser.cpp` - Parser tests for JOIN syntax
- `tests/unit/executor/test_executor.cpp` - Executor tests for join execution

## Testing Strategy

Following TDD practice (Mandatory test-driven development):

### Test Order

1. **Parser Tests** (failing first)
   - Comma-separated tables
   - Explicit CROSS JOIN
   - INNER JOIN with ON
   - LEFT JOIN with ON
   - RIGHT JOIN with ON
   - Multi-table joins
   - Ambiguous column errors

2. **Executor Tests** (failing first)
   - Cross product (2 tables)
   - Cross product (3+ tables)
   - INNER JOIN filtering
   - LEFT JOIN NULL padding
   - RIGHT JOIN NULL padding
   - Qualified column references
   - Duplicate column handling

3. **Integration Tests**
   - JOIN with WHERE
   - JOIN with GROUP BY/HAVING
   - JOIN with ORDER BY/LIMIT
   - Complex milestone query

### Test Cases

```cpp
// Example test cases

TEST_CASE("Parse comma-separated tables") {
    Parser parser(lexer("SELECT * FROM a, b"));
    auto stmt = parser.parse();
    REQUIRE(stmt.success);
    REQUIRE(stmt.value->joins().size() == 1);
    REQUIRE(stmt.value->joins()[0]->joinType() == JoinType::CROSS);
}

TEST_CASE("Execute cross join") {
    // Setup tables
    catalog.createTable("a", schema_a);
    catalog.createTable("b", schema_b);
    // Insert test data
    
    Executor executor(catalog);
    auto result = executor.execute(select_stmt);
    
    REQUIRE(result.size() == 4);  // 2x2
}

TEST_CASE("Inner join with condition") {
    // Setup tables with partial match
    
    Executor executor(catalog);
    auto result = executor.execute(inner_join_stmt);
    
    REQUIRE(result.size() == 2);  // Only matching rows
}

TEST_CASE("Left join with NULL padding") {
    // Setup tables with unmatched rows
    
    Executor executor(catalog);
    auto result = executor.execute(left_join_stmt);
    
    REQUIRE(result.size() == 3);
    REQUIRE(result[2].get(2).isNull());  // NULL from unmatched b row
}
```

## Success Criteria

Phase 2.5 is complete when:

- ✅ **F2-21**: `SELECT * FROM a, b` returns cross product  
- ✅ **F2-22**: `SELECT * FROM a INNER JOIN b ON a.id = b.id` filters by condition  
- ✅ **F2-23**: `SELECT * FROM a LEFT JOIN b ON a.id = b.id` pads with NULLs  
- ✅ **F2-24**: `SELECT * FROM a RIGHT JOIN b ON a.id = b.id` works correctly  
- ✅ **F2-25**: `SELECT * FROM a JOIN b ON ... JOIN c ON ...` chains joins  
- ✅ **Duplicate columns**: `SELECT *` prefixes with table names  
- ✅ **Error handling**: Ambiguous columns detected at parse time  
- ✅ **Integration**: Works with WHERE, GROUP BY, HAVING, ORDER BY, LIMIT  
- ✅ **Tests**: All TDD tests pass with >80% coverage  

### Final Milestone Verification

```sql
-- From spec: Phase 2 milestone query
SELECT 
    u.name,
    COUNT(o.id) AS order_count,
    COALESCE(SUM(o.amount), 0) AS total_amount,
    CASE WHEN SUM(o.amount) > 1000 THEN 'VIP' ELSE 'Normal' END AS level
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
WHERE u.status IN ('active', 'premium')
  AND u.name LIKE 'A%'
GROUP BY u.id, u.name
HAVING COUNT(o.id) > 0
ORDER BY total_amount DESC
LIMIT 10;
```

This query must execute successfully and return correct results.

## Future Enhancements (Out of Scope)

- Hash join optimization
- Join ordering optimization
- NATURAL JOIN
- USING clause
- FULL OUTER JOIN
- Self-join with aliases
- Subquery in FROM clause

## References

- PostgreSQL JOIN documentation
- DuckDB join implementation
- "Database Internals" by Alex Petrov (join algorithms)
- Existing SeedDB executor patterns
