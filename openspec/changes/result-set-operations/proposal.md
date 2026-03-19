## Why

SeedDB currently supports basic SELECT statements but lacks essential result set manipulation features. Users need ORDER BY for sorting results, LIMIT/OFFSET for pagination, aliases for readability, and DISTINCT for deduplication. These are fundamental SQL capabilities required for practical query usage.

## What Changes

- Add ORDER BY clause support with multi-column sorting (ASC/DESC)
- Add LIMIT and OFFSET clauses for result pagination
- Add column alias support (AS keyword)
- Add SELECT DISTINCT for duplicate row elimination
- Add table alias support for cleaner queries

## Capabilities

### New Capabilities

- `order-by`: ORDER BY clause parsing and execution with multi-column, direction (ASC/DESC) support
- `limit-offset`: LIMIT and OFFSET clause parsing and execution for result pagination
- `column-alias`: Column alias support in SELECT list with AS keyword
- `select-distinct`: DISTINCT keyword support for duplicate elimination in result sets
- `table-alias`: Table alias support in FROM clause for query readability

### Modified Capabilities

<!-- No existing spec-level requirements are changing -->

## Impact

- **Parser**: Extend SELECT statement parsing to handle new clauses and keywords
- **AST**: Add new AST node types for ORDER BY, LIMIT, OFFSET, aliases, and DISTINCT
- **Executor**: Implement result sorting, pagination, alias resolution, and deduplication logic
- **Lexer**: Add new keywords (ORDER, BY, LIMIT, OFFSET, AS, DISTINCT, ASC, DESC)
