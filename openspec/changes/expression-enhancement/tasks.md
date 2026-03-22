## 1. Parser - AST Extensions

- [x] 1.1 Add `CaseWhenClause` struct to ast.h (when_expr, then_expr)
- [x] 1.2 Add `EXPR_CASE` to `NodeType` enum
- [x] 1.3 Implement `CaseExpr` class with when_clauses vector and optional else_expr
- [x] 1.4 Add `EXPR_IN` to `NodeType` enum
- [x] 1.5 Implement `InExpr` class with expr, values vector, and negated flag
- [x] 1.6 Add `EXPR_BETWEEN` to `NodeType` enum
- [x] 1.7 Implement `BetweenExpr` class with expr, low, high, and negated flag
- [x] 1.8 Add `EXPR_LIKE` to `NodeType` enum
- [x] 1.9 Implement `LikeExpr` class with str, pattern, and negated flag
- [x] 1.10 Add `EXPR_COALESCE` to `NodeType` enum
- [x] 1.11 Implement `CoalesceExpr` class with args vector
- [x] 1.12 Add `EXPR_NULLIF` to `NodeType` enum
- [x] 1.13 Implement `NullifExpr` class with expr1 and expr2
- [x] 1.14 Add `toString()` methods for all new expression classes

## 2. Parser - Lexer Extensions

- [x] 2.1 Add `CASE` keyword and token
- [x] 2.2 Add `WHEN` keyword and token
- [x] 2.3 Add `THEN` keyword and token
- [x] 2.4 Add `ELSE` keyword and token
- [x] 2.5 Add `END` keyword and token
- [x] 2.6 Add `IN` keyword and token
- [x] 2.7 Add `BETWEEN` keyword and token
- [x] 2.8 Add `LIKE` keyword and token
- [x] 2.9 Add `COALESCE` keyword and token
- [x] 2.10 Add `NULLIF` keyword and token
- [x] 2.11 Update `token_type_name()` for new tokens

## 3. Parser - Grammar Implementation

- [x] 3.1 Implement `parseCaseExpr()` for CASE WHEN ... THEN ... ELSE ... END
- [x] 3.2 Implement `parseInExpr()` for IN (value_list) with negation support
- [x] 3.3 Implement `parseBetweenExpr()` for BETWEEN ... AND ... with negation support
- [x] 3.4 Implement `parseLikeExpr()` for LIKE pattern with negation support
- [x] 3.5 Implement `parseCoalesceExpr()` for COALESCE(arg1, arg2, ...)
- [x] 3.6 Implement `parseNullifExpr()` for NULLIF(expr1, expr2)
- [x] 3.7 Integrate new expression types into primary expression parsing
- [x] 3.8 Handle operator precedence (BETWEEN/IN/LIKE bind tighter than AND/OR)

## 4. Parser - Tests

- [x] 4.1 Add parser tests for CASE WHEN expressions (single branch)
- [x] 4.2 Add parser tests for CASE WHEN expressions (multiple branches)
- [x] 4.3 Add parser tests for CASE WHEN with ELSE clause
- [x] 4.4 Add parser tests for IN operator
- [x] 4.5 Add parser tests for NOT IN operator
- [x] 4.6 Add parser tests for BETWEEN operator
- [x] 4.7 Add parser tests for NOT BETWEEN operator
- [x] 4.8 Add parser tests for LIKE operator
- [x] 4.9 Add parser tests for NOT LIKE operator
- [x] 4.10 Add parser tests for COALESCE function
- [x] 4.11 Add parser tests for NULLIF function

## 5. Executor - Expression Evaluation

- [x] 5.1 Implement CASE WHEN evaluation (iterate clauses, return first match)
- [x] 5.2 Implement IN operator evaluation (check membership in value list)
- [x] 5.3 Implement NOT IN operator evaluation (negated membership)
- [x] 5.4 Implement BETWEEN operator evaluation (range check)
- [x] 5.5 Implement NOT BETWEEN operator evaluation (negated range)
- [x] 5.6 Implement LIKE pattern matching (% and _ wildcards)
- [x] 5.7 Implement NOT LIKE operator evaluation (negated match)
- [x] 5.8 Implement COALESCE function evaluation (return first non-NULL)
- [x] 5.9 Implement NULLIF function evaluation (return NULL if equal)

## 6. Executor - NULL Handling

- [x] 6.1 Handle NULL in CASE WHEN conditions (NULL condition is false)
- [x] 6.2 Handle NULL in IN operator (NULL in list affects result)
- [x] 6.3 Handle NULL in BETWEEN operator (NULL operand returns NULL)
- [x] 6.4 Handle NULL in LIKE operator (NULL operand returns NULL)
- [x] 6.5 Handle NULL in COALESCE (skip NULL arguments)
- [x] 6.6 Handle NULL in NULLIF (NULL comparison semantics)

## 7. Executor - Tests

- [x] 7.1 Add unit tests for CASE WHEN evaluation
- [x] 7.2 Add unit tests for IN/NOT IN evaluation
- [x] 7.3 Add unit tests for BETWEEN/NOT BETWEEN evaluation
- [x] 7.4 Add unit tests for LIKE/NOT LIKE pattern matching
- [x] 7.5 Add unit tests for COALESCE function
- [x] 7.6 Add unit tests for NULLIF function
- [x] 7.7 Add integration tests using new expressions in WHERE clause
- [x] 7.8 Add integration tests using new expressions in SELECT list
- [x] 7.9 Add NULL handling tests for all new expressions

## 8. Documentation

- [ ] 8.1 Update design doc to mark Phase 2.3 complete
- [ ] 8.2 Update project progress in seeddb-design.md
