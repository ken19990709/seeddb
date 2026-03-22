# Built-in Functions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add scalar function support (string, math, type casting) with an extensible function registry framework.

**Architecture:** Generic `FunctionCallExpr` AST node holds function name + arguments. Static `FunctionRegistry` singleton stores function metadata and implementations. Parser recognizes `IDENTIFIER(args)` syntax. Executor evaluates by looking up function in registry and calling implementation.

**Tech Stack:** C++17, Catch2 testing framework, existing SeedDB executor/parser infrastructure

---

## File Structure

| File | Action | Purpose |
|------|--------|---------|
| `src/parser/ast.h` | Modify | Add `FunctionCallExpr` class and `EXPR_FUNCTION_CALL` to `NodeType` |
| `src/parser/ast.cpp` | Modify | Add `FunctionCallExpr::toString()` implementation |
| `src/executor/function.h` | Create | Function registry interface (`FunctionRegistry`, `FunctionInfo`, `ScalarFunction`) |
| `src/executor/function.cpp` | Create | Registry implementation + all built-in functions (string, math) |
| `src/executor/CMakeLists.txt` | Modify | Add `function.cpp` to build |
| `src/parser/parser.cpp` | Modify | Add function call parsing in `parsePrimaryExpr()` |
| `src/executor/executor.cpp` | Modify | Add `EXPR_FUNCTION_CALL` case in `evaluateExpr()` switch |
| `tests/unit/executor/test_executor.cpp` | Modify | Add function unit tests |

---

## Task 1: Add FunctionCallExpr to AST

**Files:**
- Modify: `src/parser/ast.h:16-40` (NodeType enum)
- Modify: `src/parser/ast.h:430-450` (after NullifExpr class)
- Modify: `src/parser/ast.cpp` (add toString implementation)

- [ ] **Step 1: Add EXPR_FUNCTION_CALL to NodeType enum**

In `src/parser/ast.h`, add to the `NodeType` enum after `EXPR_NULLIF`:

```cpp
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
    EXPR_FUNCTION_CALL,  // New: scalar function call
    // Definitions
    COLUMN_DEF,
    TABLE_REF
};
```

- [ ] **Step 2: Add FunctionCallExpr class after NullifExpr**

In `src/parser/ast.h`, add after the `NullifExpr` class (around line 446):

```cpp
/// Scalar function call expression (e.g., UPPER(name), LENGTH(str))
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

- [ ] **Step 3: Add toString implementation in ast.cpp**

In `src/parser/ast.cpp`, add after `NullifExpr::toString()`:

```cpp
std::string FunctionCallExpr::toString() const {
    std::string result = name_ + "(";
    for (size_t i = 0; i < args_.size(); ++i) {
        if (i > 0) result += ", ";
        result += args_[i]->toString();
    }
    result += ")";
    return result;
}
```

- [ ] **Step 4: Build to verify AST changes compile**

Run: `cd /home/zxx/seeddb && cmake --build build --target seeddb_parser 2>&1`
Expected: Build succeeds with no errors

- [ ] **Step 5: Commit AST changes**

```bash
git add src/parser/ast.h src/parser/ast.cpp
git commit -m "feat(parser): add FunctionCallExpr AST node for scalar functions"
```

---

## Task 2: Create Function Registry Header

**Files:**
- Create: `src/executor/function.h`

- [ ] **Step 1: Create function.h with registry interface**

Create `src/executor/function.h`:

```cpp
#ifndef SEEDDB_EXECUTOR_FUNCTION_H
#define SEEDDB_EXECUTOR_FUNCTION_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/value.h"

namespace seeddb {

/// Function signature - takes evaluated arguments, returns Value
using ScalarFunction = std::function<Value(const std::vector<Value>&)>;

/// Function metadata
struct FunctionInfo {
    std::string name;        // Function name (uppercase)
    size_t min_args;         // Minimum required arguments
    size_t max_args;         // Maximum allowed arguments
    ScalarFunction impl;     // Implementation
};

/// Function registry - static lookup table for scalar functions
class FunctionRegistry {
public:
    /// Get singleton instance
    static FunctionRegistry& instance();
    
    /// Register a function
    void registerFunction(const FunctionInfo& info);
    
    /// Lookup function by name (case-insensitive)
    /// Returns nullptr if function not found
    const FunctionInfo* lookup(const std::string& name) const;
    
    /// Check if function exists
    bool hasFunction(const std::string& name) const;
    
private:
    FunctionRegistry();
    
    /// Convert name to uppercase for case-insensitive lookup
    static std::string normalizeName(const std::string& name);
    
    std::unordered_map<std::string, FunctionInfo> functions_;
};

} // namespace seeddb

#endif // SEEDDB_EXECUTOR_FUNCTION_H
```

- [ ] **Step 2: Commit function registry header**

```bash
git add src/executor/function.h
git commit -m "feat(executor): add function registry interface"
```

---

## Task 3: Implement Function Registry with Built-in Functions

**Files:**
- Create: `src/executor/function.cpp`
- Modify: `src/executor/CMakeLists.txt`

- [ ] **Step 1: Create function.cpp with registry and built-in implementations**

Create `src/executor/function.cpp`:

```cpp
#include "executor/function.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace seeddb {

// =============================================================================
// FunctionRegistry Implementation
// =============================================================================

FunctionRegistry& FunctionRegistry::instance() {
    static FunctionRegistry registry;
    return registry;
}

FunctionRegistry::FunctionRegistry() {
    // Register all built-in functions
    registerStringFunctions();
    registerMathFunctions();
}

void FunctionRegistry::registerFunction(const FunctionInfo& info) {
    std::string key = normalizeName(info.name);
    functions_[key] = info;
}

const FunctionInfo* FunctionRegistry::lookup(const std::string& name) const {
    std::string key = normalizeName(name);
    auto it = functions_.find(key);
    if (it != functions_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool FunctionRegistry::hasFunction(const std::string& name) const {
    return lookup(name) != nullptr;
}

std::string FunctionRegistry::normalizeName(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// =============================================================================
// String Functions
// =============================================================================

void FunctionRegistry::registerStringFunctions() {
    // LENGTH(str) - returns character count
    registerFunction({
        "LENGTH", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            return Value::bigint(static_cast<int64_t>(args[0].asString().length()));
        }
    });
    
    // UPPER(str) - convert to uppercase
    registerFunction({
        "UPPER", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value::varchar(s);
        }
    });
    
    // LOWER(str) - convert to lowercase
    registerFunction({
        "LOWER", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value::varchar(s);
        }
    });
    
    // TRIM(str) - remove leading/trailing whitespace
    registerFunction({
        "TRIM", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            std::string s = args[0].asString();
            // Trim leading
            size_t start = s.find_first_not_of(" \t\n\r\f\v");
            if (start == std::string::npos) return Value::varchar("");
            // Trim trailing
            size_t end = s.find_last_not_of(" \t\n\r\f\v");
            return Value::varchar(s.substr(start, end - start + 1));
        }
    });
    
    // SUBSTRING(str, start[, len]) - extract substring (1-indexed)
    registerFunction({
        "SUBSTRING", 2, 3,
        [](const std::vector<Value>& args) -> Value {
            // Any NULL arg returns NULL
            for (const auto& arg : args) {
                if (arg.isNull()) return Value::null();
            }
            
            const std::string& s = args[0].asString();
            int64_t start = args[1].asInt64();
            
            // start < 1 is invalid, return NULL
            if (start < 1) return Value::null();
            
            // Convert to 0-indexed
            size_t pos = static_cast<size_t>(start - 1);
            
            // start > length returns empty string
            if (pos >= s.length()) return Value::varchar("");
            
            // If len provided, use it; otherwise go to end
            if (args.size() == 3) {
                int64_t len = args[2].asInt64();
                if (len < 0) return Value::null();
                return Value::varchar(s.substr(pos, static_cast<size_t>(len)));
            }
            return Value::varchar(s.substr(pos));
        }
    });
    
    // CONCAT(str1, str2, ...) - concatenate strings
    registerFunction({
        "CONCAT", 2, 100,  // At least 2 args, max reasonable limit
        [](const std::vector<Value>& args) -> Value {
            std::string result;
            bool all_null = true;
            
            for (const auto& arg : args) {
                if (!arg.isNull()) {
                    all_null = false;
                    // Convert to string based on type
                    if (arg.typeId() == LogicalTypeId::VARCHAR) {
                        result += arg.asString();
                    } else {
                        result += arg.toString();
                    }
                }
            }
            
            if (all_null) return Value::null();
            return Value::varchar(result);
        }
    });
}

// =============================================================================
// Math Functions
// =============================================================================

void FunctionRegistry::registerMathFunctions() {
    // Helper to convert Value to double
    auto toDouble = [](const Value& v) -> double {
        switch (v.typeId()) {
            case LogicalTypeId::INTEGER: return static_cast<double>(v.asInt32());
            case LogicalTypeId::BIGINT: return static_cast<double>(v.asInt64());
            case LogicalTypeId::FLOAT: return static_cast<double>(v.asFloat());
            case LogicalTypeId::DOUBLE: return v.asDouble();
            default: return 0.0;
        }
    };
    
    // ABS(num) - absolute value
    registerFunction({
        "ABS", 1, 1,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            double val = toDouble(args[0]);
            if (val < 0) val = -val;
            // Preserve integer type if input was integer
            if (args[0].typeId() == LogicalTypeId::INTEGER) {
                return Value::integer(static_cast<int32_t>(val));
            }
            if (args[0].typeId() == LogicalTypeId::BIGINT) {
                return Value::bigint(static_cast<int64_t>(val));
            }
            return Value::Double(val);
        }
    });
    
    // ROUND(num[, decimals]) - round to decimals (default 0)
    registerFunction({
        "ROUND", 1, 2,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            
            int decimals = 0;
            if (args.size() == 2 && !args[1].isNull()) {
                decimals = static_cast<int>(args[1].asInt64());
            }
            
            double val = toDouble(args[0]);
            double factor = std::pow(10.0, decimals);
            val = std::round(val * factor) / factor;
            return Value::Double(val);
        }
    });
    
    // CEIL(num) - round up to integer
    registerFunction({
        "CEIL", 1, 1,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            double val = toDouble(args[0]);
            return Value::bigint(static_cast<int64_t>(std::ceil(val)));
        }
    });
    
    // FLOOR(num) - round down to integer
    registerFunction({
        "FLOOR", 1, 1,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            double val = toDouble(args[0]);
            return Value::bigint(static_cast<int64_t>(std::floor(val)));
        }
    });
    
    // MOD(num, divisor) - remainder of division
    registerFunction({
        "MOD", 2, 2,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull() || args[1].isNull()) return Value::null();
            
            double divisor = toDouble(args[1]);
            if (divisor == 0.0) return Value::null();  // Division by zero returns NULL
            
            double val = toDouble(args[0]);
            double result = std::fmod(val, divisor);
            
            // Preserve integer type if both inputs were integer types
            auto isIntegerType = [](const Value& v) {
                return v.typeId() == LogicalTypeId::INTEGER || 
                       v.typeId() == LogicalTypeId::BIGINT;
            };
            
            if (isIntegerType(args[0]) && isIntegerType(args[1])) {
                return Value::bigint(static_cast<int64_t>(result));
            }
            return Value::Double(result);
        }
    });
}

} // namespace seeddb
```

- [ ] **Step 2: Add function.cpp to CMakeLists.txt**

Modify `src/executor/CMakeLists.txt` to include `function.cpp`:

```cmake
add_library(seeddb_executor STATIC
    executor.cpp
    function.cpp
)
target_include_directories(seeddb_executor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(seeddb_executor PUBLIC seeddb_storage seeddb_parser seeddb_common)
```

- [ ] **Step 3: Build to verify function registry compiles**

Run: `cd /home/zxx/seeddb && cmake --build build --target seeddb_executor 2>&1`
Expected: Build succeeds with no errors

- [ ] **Step 4: Commit function registry implementation**

```bash
git add src/executor/function.cpp src/executor/CMakeLists.txt
git commit -m "feat(executor): implement function registry with string and math functions"
```

---

## Task 4: Add Function Call Parsing

**Files:**
- Modify: `src/parser/parser.cpp:821-842` (parsePrimaryExpr IDENTIFIER handling)

- [ ] **Step 1: Modify parsePrimaryExpr to handle function calls**

In `src/parser/parser.cpp`, modify the IDENTIFIER case in `parsePrimaryExpr()` (around line 822-839). Replace the existing identifier handling with:

```cpp
    // Column reference or function call
    if (check(TokenType::IDENTIFIER)) {
        std::string name = std::get<std::string>(current_token_.value);
        consume();

        // Check for function call: IDENTIFIER '('
        if (match(TokenType::LPAREN)) {
            // This is a function call
            auto func_expr = std::make_unique<FunctionCallExpr>(std::move(name));
            
            // Parse arguments if not empty
            if (!check(TokenType::RPAREN)) {
                do {
                    auto arg = parseExpression();
                    if (!arg.is_ok()) {
                        return arg;
                    }
                    func_expr->addArg(std::move(arg.value()));
                } while (match(TokenType::COMMA));
            }
            
            // Expect ')'
            if (!match(TokenType::RPAREN)) {
                return syntax_error<std::unique_ptr<Expr>>("Expected ')' after function arguments");
            }
            
            return Result<std::unique_ptr<Expr>>::ok(std::move(func_expr));
        }

        // Check for table.column
        if (match(TokenType::DOT)) {
            if (!check(TokenType::IDENTIFIER)) {
                return syntax_error<std::unique_ptr<Expr>>("Expected column name after '.'");
            }
            std::string col = std::get<std::string>(current_token_.value);
            consume();
            return Result<std::unique_ptr<Expr>>::ok(
                std::make_unique<ColumnRef>(std::move(name), std::move(col)));
        }

        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<ColumnRef>(std::move(name)));
    }
```

- [ ] **Step 2: Add include for function.h**

At the top of `src/parser/parser.cpp`, add (around line 3):

```cpp
#include "executor/function.h"
```

- [ ] **Step 3: Build to verify parser changes compile**

Run: `cd /home/zxx/seeddb && cmake --build build --target seeddb_parser 2>&1`
Expected: Build succeeds with no errors

- [ ] **Step 4: Commit parser changes**

```bash
git add src/parser/parser.cpp
git commit -m "feat(parser): add function call parsing in expressions"
```

---

## Task 5: Add Function Evaluation in Executor

**Files:**
- Modify: `src/executor/executor.cpp` (add case in evaluateExpr switch)

- [ ] **Step 1: Add include for function.h**

At the top of `src/executor/executor.cpp` (around line 5), add:

```cpp
#include "executor/function.h"
```

- [ ] **Step 2: Add EXPR_FUNCTION_CALL case in evaluateExpr()**

In `src/executor/executor.cpp`, find the `evaluateExpr()` function's switch statement. Add a new case after `EXPR_NULLIF` (around line 857, before the `default:` case):

```cpp
        case parser::NodeType::EXPR_FUNCTION_CALL: {
            const auto* func_expr = static_cast<const parser::FunctionCallExpr*>(expr);
            const std::string& func_name = func_expr->functionName();
            
            // Lookup function in registry
            const FunctionInfo* func_info = FunctionRegistry::instance().lookup(func_name);
            if (!func_info) {
                // Unknown function returns NULL
                return Value::null();
            }
            
            // Validate argument count
            size_t arg_count = func_expr->argCount();
            if (arg_count < func_info->min_args || arg_count > func_info->max_args) {
                // Wrong argument count returns NULL
                return Value::null();
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

- [ ] **Step 3: Build to verify executor changes compile**

Run: `cd /home/zxx/seeddb && cmake --build build --target seeddb_executor 2>&1`
Expected: Build succeeds with no errors

- [ ] **Step 4: Commit executor changes**

```bash
git add src/executor/executor.cpp
git commit -m "feat(executor): add function call evaluation in evaluateExpr"
```

---

## Task 6: Write Unit Tests for String Functions

**Files:**
- Modify: `tests/unit/executor/test_executor.cpp`

- [ ] **Step 1: Add test section for string functions**

At the end of `tests/unit/executor/test_executor.cpp`, add:

```cpp
// =============================================================================
// Scalar Function Tests - String Functions
// =============================================================================

TEST_CASE("Executor: LENGTH function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    // Create table
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    // Insert test data
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("hello")));
    executor.execute(*insert_stmt);
    
    // Parse and execute SELECT LENGTH(name) FROM test
    parser::Lexer lexer("SELECT LENGTH(name) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().size() == 1);
    REQUIRE_FALSE(exec_result.row().get(0).isNull());
    REQUIRE(exec_result.row().get(0).asInt64() == 5);
    
    executor.resetQuery();
}

TEST_CASE("Executor: UPPER function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("hello")));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT UPPER(name) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asString() == "HELLO");
    
    executor.resetQuery();
}

TEST_CASE("Executor: LOWER function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("HELLO")));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT LOWER(name) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asString() == "hello");
    
    executor.resetQuery();
}

TEST_CASE("Executor: TRIM function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("  hello  ")));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT TRIM(name) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asString() == "hello");
    
    executor.resetQuery();
}

TEST_CASE("Executor: SUBSTRING function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("hello world")));
    executor.execute(*insert_stmt);
    
    // Test SUBSTRING with 2 args
    parser::Lexer lexer("SELECT SUBSTRING(name, 1, 5) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asString() == "hello");
    
    executor.resetQuery();
}

TEST_CASE("Executor: CONCAT function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "first", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "last", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("Hello")));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("World")));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT CONCAT(first, ' ', last) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asString() == "Hello World");
    
    executor.resetQuery();
}

TEST_CASE("Executor: String function with NULL input", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    // Insert NULL
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>());  // NULL
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT UPPER(name) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).isNull());
    
    executor.resetQuery();
}
```

- [ ] **Step 2: Build and run string function tests**

Run: `cd /home/zxx/seeddb && cmake --build build && ./build/tests/unit/executor/test_executor "[function]" 2>&1`
Expected: All string function tests pass

- [ ] **Step 3: Commit string function tests**

```bash
git add tests/unit/executor/test_executor.cpp
git commit -m "test(executor): add unit tests for string functions"
```

---

## Task 7: Write Unit Tests for Math Functions

**Files:**
- Modify: `tests/unit/executor/test_executor.cpp`

- [ ] **Step 1: Add test section for math functions**

Add after the string function tests:

```cpp
// =============================================================================
// Scalar Function Tests - Math Functions
// =============================================================================

TEST_CASE("Executor: ABS function positive", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(-5)));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT ABS(val) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asInt32() == 5);
    
    executor.resetQuery();
}

TEST_CASE("Executor: ROUND function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(3.14159));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT ROUND(val, 2) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asDouble() == Approx(3.14));
    
    executor.resetQuery();
}

TEST_CASE("Executor: CEIL function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(3.14));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT CEIL(val) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asInt64() == 4);
    
    executor.resetQuery();
}

TEST_CASE("Executor: FLOOR function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(3.99));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT FLOOR(val) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asInt64() == 3);
    
    executor.resetQuery();
}

TEST_CASE("Executor: MOD function", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(10)));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT MOD(val, 3) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asInt64() == 1);
    
    executor.resetQuery();
}

TEST_CASE("Executor: MOD by zero returns NULL", "[executor][function]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "val", parser::DataTypeInfo(parser::DataType::INT)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(int64_t(10)));
    executor.execute(*insert_stmt);
    
    parser::Lexer lexer("SELECT MOD(val, 0) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).isNull());  // Division by zero returns NULL
    
    executor.resetQuery();
}
```

- [ ] **Step 2: Build and run math function tests**

Run: `cd /home/zxx/seeddb && cmake --build build && ./build/tests/unit/executor/test_executor "[function]" 2>&1`
Expected: All math function tests pass

- [ ] **Step 3: Commit math function tests**

```bash
git add tests/unit/executor/test_executor.cpp
git commit -m "test(executor): add unit tests for math functions"
```

---

## Task 8: Write Integration Tests

**Files:**
- Modify: `tests/unit/executor/test_executor.cpp`

- [ ] **Step 1: Add integration tests for functions in queries**

Add after math function tests:

```cpp
// =============================================================================
// Scalar Function Integration Tests
// =============================================================================

TEST_CASE("Executor: Function in WHERE clause", "[executor][function][integration]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("users");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert1 = std::make_unique<parser::InsertStmt>("users");
    insert1->addValues(std::make_unique<parser::LiteralExpr>(std::string("Alice")));
    executor.execute(*insert1);
    
    auto insert2 = std::make_unique<parser::InsertStmt>("users");
    insert2->addValues(std::make_unique<parser::LiteralExpr>(std::string("Bob")));
    executor.execute(*insert2);
    
    auto insert3 = std::make_unique<parser::InsertStmt>("users");
    insert3->addValues(std::make_unique<parser::LiteralExpr>(std::string("Charlie")));
    executor.execute(*insert3);
    
    // SELECT * FROM users WHERE LENGTH(name) > 3
    parser::Lexer lexer("SELECT name FROM users WHERE LENGTH(name) > 3");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    
    int count = 0;
    while (executor.hasNext()) {
        auto exec_result = executor.next();
        REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
        count++;
    }
    REQUIRE(count == 2);  // Alice (5) and Charlie (7)
    
    executor.resetQuery();
}

TEST_CASE("Executor: Nested function calls", "[executor][function][integration]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("test");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("test");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("  hello  ")));
    executor.execute(*insert_stmt);
    
    // SELECT UPPER(TRIM(name)) FROM test
    parser::Lexer lexer("SELECT UPPER(TRIM(name)) FROM test");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().get(0).asString() == "HELLO");
    
    executor.resetQuery();
}

TEST_CASE("Executor: Multiple functions in SELECT", "[executor][function][integration]") {
    Catalog catalog;
    Executor executor(catalog);
    
    auto create_stmt = std::make_unique<parser::CreateTableStmt>("products");
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    create_stmt->addColumn(std::make_unique<parser::ColumnDef>(
        "price", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    executor.execute(*create_stmt);
    
    auto insert_stmt = std::make_unique<parser::InsertStmt>("products");
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(std::string("Widget")));
    insert_stmt->addValues(std::make_unique<parser::LiteralExpr>(19.99));
    executor.execute(*insert_stmt);
    
    // SELECT UPPER(name), ROUND(price, 0) FROM products
    parser::Lexer lexer("SELECT UPPER(name), ROUND(price, 0) FROM products");
    parser::Parser parser(lexer);
    auto result = parser.parse();
    REQUIRE(result.is_ok());
    
    auto* select_stmt = dynamic_cast<parser::SelectStmt*>(result.value().get());
    REQUIRE(select_stmt != nullptr);
    
    REQUIRE(executor.prepareSelect(*select_stmt));
    REQUIRE(executor.hasNext());
    
    auto exec_result = executor.next();
    REQUIRE(exec_result.status() == ExecutionResult::Status::OK);
    REQUIRE(exec_result.row().size() == 2);
    REQUIRE(exec_result.row().get(0).asString() == "WIDGET");
    REQUIRE(exec_result.row().get(1).asDouble() == Approx(20.0));
    
    executor.resetQuery();
}
```

- [ ] **Step 2: Build and run integration tests**

Run: `cd /home/zxx/seeddb && cmake --build build && ./build/tests/unit/executor/test_executor "[integration]" 2>&1`
Expected: All integration tests pass

- [ ] **Step 3: Run all tests to ensure no regressions**

Run: `cd /home/zxx/seeddb && ./build/tests/unit/executor/test_executor 2>&1`
Expected: All tests pass (no regressions)

- [ ] **Step 4: Commit integration tests**

```bash
git add tests/unit/executor/test_executor.cpp
git commit -m "test(executor): add integration tests for scalar functions"
```

---

## Task 9: Final Verification and Cleanup

- [ ] **Step 1: Run full test suite**

Run: `cd /home/zxx/seeddb && cmake --build build && ctest --test-dir build --output-on-failure 2>&1`
Expected: All tests pass

- [ ] **Step 2: Verify no compiler warnings**

Run: `cd /home/zxx/seeddb && cmake --build build 2>&1 | grep -i warning`
Expected: No warnings (empty output)

- [ ] **Step 3: Final commit (if any remaining changes)**

```bash
git status
# If there are uncommitted changes:
git add -A
git commit -m "chore: finalize Phase 2.4 built-in functions implementation"
```

---

## Critical Dependencies

```
Task 1 (AST) ─────────────┐
                          ├──> Task 4 (Parser)
Task 2 (Function.h) ──────┤
                          │
Task 3 (Function.cpp) ────┼──> Task 5 (Executor)
                          │
                          ├──> Task 6 (String Tests)
                          │
                          ├──> Task 7 (Math Tests)
                          │
                          └──> Task 8 (Integration Tests)
```

**Sequential order required:**
1. Tasks 1, 2, 3 can run in parallel (independent)
2. Task 4 depends on Task 1
3. Task 5 depends on Tasks 2, 3, 4
4. Tasks 6, 7, 8 depend on Task 5
5. Task 9 depends on all previous tasks
