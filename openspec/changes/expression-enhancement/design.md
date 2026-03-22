## Context

SeedDB is an educational database project currently in Phase 2 (SQL Enhancement). Phase 2.1 (Result Set Operations) and Phase 2.2 (Aggregation and Grouping) are complete. This design covers Phase 2.3 - Expression Enhancement.

The current expression system supports:
- Binary expressions: arithmetic (+, -, *, /), comparison (=, <>, <, >, <=, >=), logical (AND, OR)
- Unary expressions: NOT, arithmetic negation
- IS NULL / IS NOT NULL checks
- Column references and literals

This change extends the expression system to support more complex SQL expressions commonly used in real-world queries.

## Goals / Non-Goals

**Goals:**
- Implement CASE WHEN expressions with multiple WHEN clauses and optional ELSE
- Implement IN operator for set membership testing
- Implement NOT IN operator (negated membership)
- Implement BETWEEN operator for range checks
- Implement LIKE operator with % and _ wildcard support
- Implement COALESCE function for NULL handling
- Implement NULLIF function for conditional NULL results
- Handle NULL values correctly in all new expressions

**Non-Goals:**
- Regular expression matching (REGEXP/RLIKE) - future work
- CASE statement (procedural) vs CASE expression - only expression form
- ESCAPE clause for LIKE - future enhancement
- Row-value constructors in IN (e.g., `(a, b) IN ((1, 2), (3, 4))`) - future work
- Subqueries in IN clause - future work (Phase 7)

## Decisions

### Decision 1: CASE WHEN AST Representation

**Choice:** Add `CaseExpr` node with vector of WHEN-THEN pairs and optional ELSE

**Rationale:** CASE WHEN is a complex multi-branch expression. Each branch has a condition and result. The ELSE clause is optional (defaults to NULL).

**Design:**
```cpp
struct CaseWhenClause {
    std::unique_ptr<Expr> when_expr;  // Condition
    std::unique_ptr<Expr> then_expr;  // Result if condition is true
};

class CaseExpr : public Expr {
    std::vector<CaseWhenClause> when_clauses_;
    std::unique_ptr<Expr> else_expr_;  // Optional, defaults to NULL
};
```

**Alternatives considered:**
- Nested IF-THEN-ELSE: Less readable, harder to parse
- Single expression with ternary operator: Doesn't scale to multiple branches

### Decision 2: IN Operator Implementation

**Choice:** Add `InExpr` node with expression and value list

**Rationale:** IN operator checks membership in a list of values. It's syntactic sugar for OR-chained equality comparisons, but more readable and potentially optimizable.

**Design:**
```cpp
class InExpr : public Expr {
    std::unique_ptr<Expr> expr_;       // The expression to test
    std::vector<std::unique_ptr<Expr>> values_;  // Value list
    bool negated_;                     // true for NOT IN
};
```

**Evaluation:**
- Evaluate `expr_`, then check if result equals any value in `values_`
- NULL handling: If `expr_` is NULL, result is NULL (unknown)
- If any value in list is NULL and no match found, result is NULL (not false)
- NOT IN with NULL: If any list value is NULL, result is NULL or false (never true)

### Decision 3: BETWEEN Operator Implementation

**Choice:** Add `BetweenExpr` node with value, low, and high bounds

**Rationale:** BETWEEN is syntactic sugar for `value >= low AND value <= high`, but more readable and allows for single evaluation of the value expression.

**Design:**
```cpp
class BetweenExpr : public Expr {
    std::unique_ptr<Expr> expr_;   // Value to test
    std::unique_ptr<Expr> low_;    // Lower bound (inclusive)
    std::unique_ptr<Expr> high_;   // Upper bound (inclusive)
    bool negated_;                 // true for NOT BETWEEN
};
```

**Evaluation:**
- Equivalent to `expr_ >= low_ AND expr_ <= high_`
- NULL handling: If any operand is NULL, result is NULL
- NOT BETWEEN is `NOT (expr BETWEEN low AND high)`

### Decision 4: LIKE Operator Implementation

**Choice:** Add `LikeExpr` node with string and pattern

**Rationale:** LIKE provides pattern matching with SQL-style wildcards. It's a string-specific operation that returns boolean.

**Design:**
```cpp
class LikeExpr : public Expr {
    std::unique_ptr<Expr> str_;      // String to match
    std::unique_ptr<Expr> pattern_;  // Pattern with % and _
    bool negated_;                   // true for NOT LIKE
};
```

**Wildcard Semantics:**
- `%` matches any sequence of zero or more characters
- `_` matches exactly one character
- Pattern is case-sensitive (PostgreSQL default)
- NULL handling: If either operand is NULL, result is NULL

**Implementation:**
Convert SQL LIKE pattern to regex or implement custom matcher:
```cpp
// Simple implementation: convert % to .*, _ to .
std::string likeToRegex(const std::string& pattern);
```

### Decision 5: COALESCE Function Implementation

**Choice:** Add `CoalesceExpr` node with argument list

**Rationale:** COALESCE is a variadic function that returns the first non-NULL argument. It's standard SQL and commonly used for default values.

**Design:**
```cpp
class CoalesceExpr : public Expr {
    std::vector<std::unique_ptr<Expr>> args_;
};
```

**Evaluation:**
- Evaluate arguments left-to-right
- Return first non-NULL result
- If all arguments are NULL, return NULL
- Type coercion: Result type is the "common type" of all arguments

### Decision 6: NULLIF Function Implementation

**Choice:** Add `NullifExpr` node with two arguments

**Rationale:** NULLIF returns NULL if two values are equal, otherwise returns the first value. Useful for avoiding division-by-zero and other edge cases.

**Design:**
```cpp
class NullifExpr : public Expr {
    std::unique_ptr<Expr> expr1_;
    std::unique_ptr<Expr> expr2_;
};
```

**Evaluation:**
- If `expr1_ == expr2_`, return NULL
- Otherwise, return `expr1_`
- If either expression is NULL, return `expr1_` (NULL == anything is not true)

### Decision 7: NodeType Enum Extension

**Choice:** Add new node types to the existing NodeType enum

**Design:**
```cpp
enum class NodeType {
    // ... existing types ...
    EXPR_CASE,        // CaseExpr
    EXPR_IN,          // InExpr
    EXPR_BETWEEN,     // BetweenExpr
    EXPR_LIKE,        // LikeExpr
    EXPR_COALESCE,    // CoalesceExpr
    EXPR_NULLIF,      // NullifExpr
};
```

## Risks / Trade-offs

### Risk: Pattern Matching Performance
- **Risk:** Naive regex conversion for LIKE may be slow for complex patterns
- **Mitigation:** Acceptable for educational project; could optimize with specialized matcher in future

### Risk: IN List Size
- **Risk:** Large IN lists could consume significant memory
- **Mitigation:** Acceptable for current scale; could use hash set for membership testing

### Trade-off: Type Coercion in COALESCE
- **Trade-off:** Determining common type for COALESCE arguments is complex
- **Mitigation:** Use first argument's type for now; add coercion rules later if needed

### Risk: Expression Depth
- **Risk:** Deeply nested CASE expressions could cause stack overflow in recursive evaluation
- **Mitigation:** Acceptable for typical queries; could add depth limit in future
