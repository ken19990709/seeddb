## Why

SeedDB currently supports basic SQL expressions including arithmetic operations, comparisons, and NULL checks (IS NULL/IS NOT NULL). However, it lacks several commonly-used expression constructs that are essential for real-world SQL queries:

- **Conditional logic**: No way to return different values based on conditions (CASE WHEN)
- **Set membership**: No IN/NOT IN operators for checking membership in value lists
- **Range checks**: No BETWEEN operator for inclusive range comparisons
- **Pattern matching**: No LIKE operator for string pattern matching with wildcards
- **NULL handling**: No COALESCE or NULLIF functions for sophisticated NULL handling

These limitations make it difficult to write practical queries that need conditional logic, filtering by lists, or string pattern matching.

## What Changes

- **CASE WHEN expressions**: Full CASE WHEN ... THEN ... ELSE ... END support with multiple WHEN clauses
- **IN operator**: `expr IN (val1, val2, ...)` for set membership tests
- **NOT IN operator**: `expr NOT IN (val1, val2, ...)` for negated membership tests
- **BETWEEN operator**: `expr BETWEEN low AND high` for inclusive range checks
- **LIKE operator**: `str LIKE pattern` with `%` and `_` wildcard support
- **COALESCE function**: `COALESCE(val1, val2, ...)` returns first non-NULL argument
- **NULLIF function**: `NULLIF(val1, val2)` returns NULL if values are equal

## Capabilities

### New Capabilities

- `case-when-expression`: Conditional CASE WHEN expressions with multiple WHEN clauses and optional ELSE
- `in-operator`: IN operator for checking if a value matches any value in a list
- `not-in-operator`: NOT IN operator for checking if a value does not match any value in a list
- `between-operator`: BETWEEN operator for inclusive range comparisons
- `like-operator`: LIKE operator for pattern matching with `%` (any characters) and `_` (single character) wildcards
- `coalesce-function`: COALESCE function returning first non-NULL argument
- `nullif-function`: NULLIF function returning NULL when two values are equal

### Modified Capabilities

- `expression-evaluation`: Extended to support new expression types

## Impact

### Parser Changes

- Add new AST node types for CASE, IN, BETWEEN, LIKE expressions
- Extend lexer with BETWEEN, LIKE, CASE, WHEN, THEN, ELSE, END, IN keywords
- Add parsing rules for new expression syntax

### Executor Changes

- Implement CASE WHEN evaluation with condition checking
- Implement IN/NOT IN as set membership tests
- Implement BETWEEN as range comparison (syntactic sugar for >= AND <=)
- Implement LIKE pattern matching with wildcard support
- Implement COALESCE as NULL-coalescing expression
- Implement NULLIF as equality comparison with NULL result

### Type System

- CASE WHEN result type determined by THEN/ELSE branches
- IN/BETWEEN return BOOLEAN type
- LIKE returns BOOLEAN type
- COALESCE returns first non-NULL argument type
- NULLIF returns argument type or NULL

### Dependencies

- No new external dependencies required
- Builds on existing expression evaluation framework
