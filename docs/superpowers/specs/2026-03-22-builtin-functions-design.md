# Phase 2.4: Built-in Functions Design

> Design Date: 2026-03-22

## 1. Overview

### 1.1 Scope
This phase adds scalar function support to SeedDB, including string functions, math functions, type casting, and an extensible function framework.

### 1.2 Key Decisions
| Decision | Choice | Rationale |
|----------|--------|----------|
| AST Representation | Generic `FunctionCallExpr` | PostgreSQL/DuckDB pattern; easy extension |
| Function Dispatch | Static function table | Balance of simplicity and extensibility |
| Validation | Two-phase (parse + execute) | Better error messages while keeping parse independent of data |
| Error Handling | Silent NULL return | PostgreSQL style; simpler implementation for educational DB |

### 1.3 Functions to Implement
| Category | Functions |
|----------|----------|
| String | `LENGTH`, `UPPER`, `LOWER`, `TRIM`, `SUBSTRING`, `CONCAT` |
| Math | `ABS`, `ROUND`, `CEIL`, `FLOOR`, `MOD` |
| Type | `CAST` (via existing CAST expression syntax) |

## 2. Architecture

### 2.1 AST Design

**New AST Node Type:**
```cpp
enum class NodeType {
    // ... existing types ...
    EXPR_FUNCTION_CALL,  // New: scalar function call
};

/// Scalar function call expression
class FunctionCallExpr : public Expr {
public:
    explicit FunctionCallExpr(std::string name) : name_(std::move(name)) {}
    
    NodeType type() const override { return NodeType::EXPR_FUNCTION_CALL; }
    std::string toString() const override;
    
    const std::string& functionName() const { return name_; }
    const auto& args() const { return args_; }
    size_t argCount() const { return args_.size(); }
    
    void addArg(std::unique_ptr<Expr> arg) { args_.push_back(std::move(arg)); }
    
private:
    std::string name_;
    std::vector<std::unique_ptr<Expr>> args_;
};
```

### 2.2 Function Registry

**New Files:**
- `src/executor/function.h` - Function registry interface
- `src/executor/function.cpp` - Function registry implementation and built-in function registration

**Function Registry Interface:**
```cpp
namespace seeddb {

/// Function signature - takes evaluated arguments, returns Value
using ScalarFunction = std::function<Value(const std::vector<Value>&)>;

/// Function metadata
struct FunctionInfo {
    std::string name;           // Function name (uppercase)
    size_t min_args;            // Minimum required arguments
    size_t max_args;            // Maximum allowed arguments
    ScalarFunction impl;         // Implementation
};

/// Function registry - static lookup table
class FunctionRegistry {
public:
    static FunctionRegistry& instance();
    
    void registerFunction(const FunctionInfo& info);
    const FunctionInfo* lookup(const std::string& name) const;
    bool hasFunction(const std::string& name) const;
    
private:
    FunctionRegistry();
    std::unordered_map<std::string, FunctionInfo> functions_;
};

} // namespace seeddb
```

### 2.3 Parser Changes

**File: `src/parser/parser.cpp`**

Add function call parsing in expression parsing logic:

```cpp
// In parsePrimaryExpr() or similar location:
// Handle function calls: IDENTIFIER '(' args ')'
if (match(TokenType::IDENTIFIER)) {
    std::string func_name = currentToken().valueAsString();
    advance();
    
    if (match(TokenType::LEFT_PAREN)) {
        // This is a function call
        advance();
        auto func_expr = std::make_unique<FunctionCallExpr>(func_name);
        
        // Parse arguments
        if (!match(TokenType::RIGHT_PAREN)) {
            // First argument
            func_expr->addArg(parseExpression());
            
            // Additional arguments
            while (match(TokenType::COMMA)) {
                advance();
                func_expr->addArg(parseExpression());
            }
            
            expect(TokenType::RIGHT_PAREN);
        }
        advance();  // consume ')'
        
        return func_expr;
    } else {
        // Not a function call, handle as identifier/column ref
        // ... existing column ref logic
    }
}
```

### 2.4 Executor Changes

**File: `src/executor/executor.cpp`**

Add case in `evaluateExpr()`:

```cpp
case parser::NodeType::EXPR_FUNCTION_CALL: {
    const auto* func_expr = static_cast<const parser::FunctionCallExpr*>(expr);
    const std::string& func_name = func_expr->functionName();
    
    // Lookup function in registry
    const FunctionInfo* func_info = FunctionRegistry::instance().lookup(func_name);
    if (!func_info) {
        return Value::null();  // Unknown function
    }
    
    // Validate argument count
    size_t arg_count = func_expr->argCount();
    if (arg_count < func_info->min_args || arg_count > func_info->max_args) {
        return Value::null();  // Wrong argument count
    }
    
    // Evaluate arguments
    std::vector<Value> args;
    args.reserve(arg_count);
    for (const auto& arg : func_expr->args()) {
        args.push_back(evaluateExpr(arg.get(), row, schema));
    }
    
    // Call function implementation
    return func_info->impl(args);
}
```

## 3. Function Specifications

### 3.1 String Functions

| Function | Signature | Description | NULL Behavior |
|----------|-----------|-------------|---------------|
| `LENGTH` | `LENGTH(str)` | Returns number of characters in string | NULL → NULL |
| `UPPER` | `UPPER(str)` | Converts string to uppercase | NULL → NULL |
| `LOWER` | `LOWER(str)` | Converts string to lowercase | NULL → NULL |
| `TRIM` | `TRIM(str)` | Removes leading and trailing whitespace | NULL → NULL |
| `SUBSTRING` | `SUBSTRING(str, start[, len])` | Extracts substring (1-indexed, SQL standard) | Any NULL arg → NULL; start < 1 → NULL |
| `CONCAT` | `CONCAT(str1, str2, ...)` | Concatenates strings | Skips NULL args; all NULL → NULL |

**SUBSTRING Details:**
- `start` is 1-indexed (SQL standard, not 0-indexed like C++)
- If `len` omitted: returns from `start` to end of string
- If `start` > string length: returns empty string (not NULL)
- If `start` < 1: returns NULL (invalid position)

### 3.2 Math Functions

| Function | Signature | Description | NULL Behavior |
|----------|-----------|-------------|---------------|
| `ABS` | `ABS(num)` | Returns absolute value | NULL → NULL |
| `ROUND` | `ROUND(num[, decimals])` | Rounds to specified decimal places (default 0) | Any NULL → NULL |
| `CEIL` | `CEIL(num)` | Rounds up to nearest integer | NULL → NULL |
| `FLOOR` | `FLOOR(num)` | Rounds down to nearest integer | NULL → NULL |
| `MOD` | `MOD(num, divisor)` | Returns remainder of division | Any NULL → NULL; divisor = 0 → NULL |

**Math Function Type Handling:**
- Accept INTEGER, BIGINT, FLOAT, DOUBLE
- Return INTEGER for integer inputs, DOUBLE for float inputs
- `ROUND` always returns DOUBLE for precision preservation

### 3.3 Type Conversion (CAST)

CAST is handled via the existing `CAST(expr AS type)` SQL syntax, not as a function call.

**Supported Conversions:**
| From | To | Behavior |
|------|------|---------|
| VARCHAR | INTEGER | Parse as integer; invalid → NULL |
| VARCHAR | BIGINT | Parse as bigint; invalid → NULL |
| VARCHAR | DOUBLE | Parse as double; invalid → NULL |
| VARCHAR | BOOLEAN | 'true'/'false' (case-insensitive) → BOOLEAN; invalid → NULL |
| INTEGER | VARCHAR | Convert to string representation |
| DOUBLE | VARCHAR | Convert to string representation |
| BOOLEAN | VARCHAR | 'true' or 'false' |
| Any | VARCHAR | Fallback to string representation |

## 4. Implementation Tasks

### 4.1 Task Breakdown

| Task | Files | Estimate |
|------|-------|----------|
| T1: Add FunctionCallExpr to AST | ast.h, ast.cpp | 0.5 day |
| T2: Add EXPR_FUNCTION_CALL to lexer keywords | token.h, keywords.h | 0.25 day |
| T3: Create function registry | function.h, function.cpp | 0.5 day |
| T4: Update CMakeLists.txt for executor | executor/CMakeLists.txt | 0.25 day |
| T5: Implement string functions | function.cpp | 0.5 day |
| T6: Implement math functions | function.cpp | 0.5 day |
| T7: Implement CAST execution | executor.cpp | 0.5 day |
| T8: Add function call parsing | parser.cpp | 1 day |
| T9: Add function evaluation in executor | executor.cpp | 0.5 day |
| T10: Unit tests for all functions | test_executor.cpp | 1 day |
| T11: Integration tests (functions in queries) | test_executor.cpp | 0.5 day |

**Total Estimate: 4 days**

### 4.2 Critical Dependencies

```
T1 (AST) ──> T8 (Parser)
                    ──> T9 (Executor)
                    
T2 (Lexer) ──> T8 (Parser)

T3 (Registry) ──> T5 (String functions)
             ──> T6 (Math functions)
             ──> T9 (Executor evaluation)

T4 (CMake) ──> T3 (function.cpp compiles)

T5,T6 (Functions) ──> T10 (Unit tests)

T7 (CAST) ──> T10 (Unit tests)

T8,T9 (Implementation) ──> T11 (Integration tests)
```

## 5. Test Strategy

### 5.1 Unit Tests

Each function tested in isolation:
- Basic functionality (normal inputs)
- NULL input handling
- Edge cases (empty string, zero, negative numbers)
- Type coercion (integer to string for CONCAT, etc.)

### 5.2 Integration Tests

Functions in SQL query context:
- In SELECT list: `SELECT UPPER(name) FROM users`
- In WHERE clause: `SELECT * FROM users WHERE LENGTH(name) > 5`
- Nested function calls: `SELECT UPPER(TRIM(name)) FROM users`
- Multiple functions: `SELECT CONCAT(UPPER(first), ' ', UPPER(last)) FROM users`
- With aggregates: `SELECT COUNT(*), UPPER(status) FROM users GROUP BY status`

## 6. Future Extensibility

The function registry pattern supports future additions:

**Planned Future Functions:**
- Date/Time: `NOW()`, `CURRENT_DATE()`, `EXTRACT()`
- Conditional: `IFNULL()` (alias for COALESCE with 2 args)
- String: `REPLACE()`, `POSITION()`, `LPAD()`, `RPAD()`
- Math: `POWER()`, `SQRT()`, `LOG()`
- Aggregate: User-defined aggregates (UDAF)

**Extension Points:**
- `FunctionRegistry::registerFunction()` allows adding new functions without modifying executor code
- Function metadata includes min/max args for validation
- Implementation uses `std::function` for flexibility
