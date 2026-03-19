## 1. Lexer - Add New Keywords

- [x] 1.1 Add keywords: ORDER, BY, ASC, DESC to lexer keywords map
- [x] 1.2 Add keywords: LIMIT, OFFSET to lexer keywords map
- [x] 1.3 Add keyword: DISTINCT to lexer keywords map
- [x] 1.4 Add keyword: AS to lexer keywords map (if not present)
- [x] 1.5 Write unit tests for new keyword tokenization

## 2. AST - New Node Types and Structures

- [x] 2.1 Create SortDirection enum (ASC, DESC) in ast.h
- [x] 2.2 Create OrderByItem struct with expr and direction fields
- [x] 2.3 Create SelectItem struct with expr and alias fields
- [x] 2.4 Update SelectStmt: replace columns vector with SelectItem vector
- [x] 2.5 Add order_by_ vector of OrderByItem to SelectStmt
- [x] 2.6 Add limit_ and offset_ optional fields to SelectStmt
- [x] 2.7 Add distinct_ boolean flag to SelectStmt
- [x] 2.8 Add accessor methods for new SelectStmt fields
- [x] 2.9 Update SelectStmt::toString() for new clauses
- [x] 2.10 Write unit tests for new AST structures

## 3. Parser - Column Alias Support

- [x] 3.1 Modify parseSelectList to return vector of SelectItem
- [x] 3.2 Parse optional AS keyword followed by alias identifier
- [x] 3.3 Support implicit alias (identifier without AS)
- [x] 3.4 Write parser tests for column aliases

## 4. Parser - Table Alias Support

- [x] 4.1 Extend parseTableRef to parse optional AS keyword
- [x] 4.2 Support implicit table alias (identifier without AS)
- [x] 4.3 Write parser tests for table aliases

## 5. Parser - DISTINCT Support

- [x] 5.1 Check for DISTINCT keyword after SELECT
- [x] 5.2 Set distinct flag on SelectStmt when present
- [x] 5.3 Write parser tests for SELECT DISTINCT

## 6. Parser - ORDER BY Support

- [x] 6.1 Add parseOrderByClause method
- [x] 6.2 Parse ORDER BY keywords
- [x] 6.3 Parse comma-separated list of sort items
- [x] 6.4 Parse optional ASC/DESC direction for each item
- [x] 6.5 Support qualified column names (table.column)
- [x] 6.6 Write parser tests for ORDER BY clauses

## 7. Parser - LIMIT/OFFSET Support

- [x] 7.1 Add parseLimitClause method
- [x] 7.2 Parse LIMIT keyword followed by integer literal
- [x] 7.3 Parse optional OFFSET keyword followed by integer literal
- [x] 7.4 Validate non-negative values at parse time
- [x] 7.5 Write parser tests for LIMIT and OFFSET

## 8. Executor - DISTINCT Implementation

- [x] 8.1 Add row deduplication logic after projection
- [x] 8.2 Implement row comparison for DISTINCT
- [x] 8.3 Handle NULL values as equal for DISTINCT purposes
- [x] 8.4 Write executor tests for DISTINCT

## 9. Executor - ORDER BY Implementation

- [x] 9.1 Add sorting logic after DISTINCT (if applicable)
- [x] 9.2 Create multi-column comparator for sorting
- [x] 9.3 Support ascending and descending sort per column
- [x] 9.4 Handle NULL values in sorting (NULLS LAST default)
- [x] 9.5 Write executor tests for ORDER BY

## 10. Executor - LIMIT/OFFSET Implementation

- [x] 10.1 Apply OFFSET by skipping rows after sorting
- [x] 10.2 Apply LIMIT by truncating result set
- [x] 10.3 Handle edge cases (offset > rows, limit 0)
- [x] 10.4 Write executor tests for LIMIT and OFFSET

## 11. Executor - Alias Resolution

- [x] 11.1 Build alias map during column projection
- [x] 11.2 Support ORDER BY referencing column aliases
- [x] 11.3 Resolve table aliases for qualified column references
- [x] 11.4 Write executor tests for alias resolution

## 12. Integration Testing

- [x] 12.1 End-to-end test: SELECT with column aliases
- [x] 12.2 End-to-end test: SELECT DISTINCT with multiple columns
- [x] 12.3 End-to-end test: ORDER BY with multiple columns and directions
- [x] 12.4 End-to-end test: LIMIT/OFFSET pagination
- [x] 12.5 End-to-end test: Combined clauses (DISTINCT + ORDER BY + LIMIT)
- [x] 12.6 End-to-end test: Table alias with qualified column references
