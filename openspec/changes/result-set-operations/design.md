## Context

SeedDB is an educational database that currently supports basic SQL operations. The parser handles SELECT statements with column lists, FROM clause, and WHERE clause. The [SelectStmt](file:///home/zxx/seeddb/src/parser/ast.h) class captures these elements but lacks support for result set manipulation. [TableRef](file:///home/zxx/seeddb/src/parser/ast.h) already supports table aliases, which we'll leverage.

Current SELECT flow: Parser → SelectStmt AST → Executor scans table, filters with WHERE → Returns rows.

## Goals / Non-Goals

**Goals:**
- Support ORDER BY with multiple columns and ASC/DESC directions
- Support LIMIT and OFFSET for result pagination
- Support column aliases in SELECT list (AS keyword)
- Support SELECT DISTINCT for row deduplication
- Integrate table alias resolution throughout query processing

**Non-Goals:**
- ORDER BY with expressions (e.g., `ORDER BY a + b`) - columns only for Phase 2.1
- NULLS FIRST/LAST ordering control
- Window functions or partitioned ordering
- Subqueries in LIMIT/OFFSET

## Decisions

### D1: Extend SelectStmt for new clauses

**Decision**: Add fields to the existing `SelectStmt` class rather than creating wrapper classes.

**Rationale**: Keeps the AST simple and avoids unnecessary indirection. The clauses are tightly coupled to SELECT semantics.

**Alternatives considered**:
- Separate clause classes composed into SelectStmt - adds complexity without benefit for this scope

### D2: Column alias representation

**Decision**: Create `SelectItem` wrapper struct containing expression + optional alias, replacing the raw `Expr*` in columns list.

```cpp
struct SelectItem {
    std::unique_ptr<Expr> expr;
    std::string alias;  // empty if no alias
};
```

**Rationale**: Keeps alias logically paired with the expression it names.

**Alternatives considered**:
- Store aliases in parallel vector - error-prone, harder to maintain correspondence
- Add alias field to Expr base - pollutes unrelated expression types

### D3: ORDER BY representation

**Decision**: Create `OrderByItem` struct with column reference and direction enum.

```cpp
enum class SortDirection { ASC, DESC };
struct OrderByItem {
    std::unique_ptr<Expr> expr;  // Column reference for Phase 2.1
    SortDirection direction = SortDirection::ASC;
};
```

**Rationale**: Clean separation of sort key from direction. Enum prevents string comparison.

### D4: Execution order

**Decision**: Execute in standard SQL order: FROM → WHERE → SELECT (projection) → DISTINCT → ORDER BY → LIMIT/OFFSET

**Rationale**: Matches SQL semantics. DISTINCT before ORDER BY allows sorting to operate on deduplicated set. Aliases resolved during projection are available for ORDER BY.

### D5: In-memory sorting strategy

**Decision**: Use `std::sort` with custom comparator for ORDER BY execution.

**Rationale**: Simple, correct, and sufficient for educational purposes. SeedDB operates on in-memory data where standard library sorting is efficient.

**Alternatives considered**:
- External merge sort - overkill for in-memory educational database
- Index-based sorting - no index support yet

### D6: DISTINCT implementation

**Decision**: Use row-level comparison with temporary set for deduplication.

**Rationale**: Straightforward O(n) deduplication using hash set or sorted uniquification.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Large result sets causing memory issues with ORDER BY | LIMIT/OFFSET naturally limits result size; educational scope means small datasets |
| Alias resolution complexity with table.column references | Resolve aliases during SELECT projection before ORDER BY |
| Multi-column ORDER BY performance | Standard library sort handles multi-key comparison efficiently |
| DISTINCT on rows with many columns | Hash entire row; acceptable for educational scale |
