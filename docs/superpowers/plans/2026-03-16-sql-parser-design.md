# SQL Parser implementation plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) then superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement SQL Parser (Phase 2) to token stream from abstract syntax trees (ASTs) for query executor
**Architecture:** Hand-written recursive descent parser using zero external dependencies, with a AST traversal, future visitor pattern. Clear responsibility: one file per responsibility. Files that live together should live together. Split by responsibility, not by technical layer. In existing codebases, follow established patterns. If files grow unwieldy, including a split in plan is reasonable.
        - Design units with clear boundaries and well-defined interfaces. Each unit is a focused, focused file that can hold in context at once, keeping it manageable.
        - Testable, and with frequent commits
**Tech Stack:** C++17, Catch2 testing framework, CMake/Nake
**Files to create/modify:**
- Create: `src/parser/ast.h`
- Create: `src/parser/ast.cpp`
- Create: `tests/unit/parser/test_ast.cpp`
- Modify: `src/parser/CMakeLists.txt`
- modify: `tests/CMakeLists.txt`
- Update: `docs/superpowers/progress.md`

## Chunk 1: AST Foundation
Define node types, data types, and base AST classes.

### Task 1: Create AST header with node types

**Files:**
- Create: `src/parser/ast.h`

- [ ] **Step 1: Write failing test for node types**

Create test file with basic type checks:

```cpp
// tests/unit/parser/test_ast.cpp
#include <catch2/catch_test_macros.hpp>
#include "parser/ast.h"

using namespace seeddb::parser;

TEST_CASE("AST: NodeType enum exists", "[ast]") {
    // Statement types
    REQUIRE(static_cast<int>(NodeType::STMT_CREATE_TABLE) == 0);
    REQUIRE(static_cast<int>(NodeType::STMT_DROP_TABLE) == 1);
    REQUIRE(static_cast<int>(NodeType::STMT_INSERT) == 2);
    REQUIRE(static_cast<int>(NodeType::STMT_SELECT) == 3);
    REQUIRE(static_cast<int>(NodeType::STMT_UPDATE) == 4);
    REQUIRE(static_cast<int>(NodeType::STMT_DELETE) == 5);

    // Expression types
    REQUIRE(static_cast<int>(NodeType::EXPR_BINARY) == 6);
    REQUIRE(static_cast<int>(NodeType::EXPR_UNARY) == 7);
    REQUIRE(static_cast<int>(NodeType::EXPR_LITERAL) == 8);
    REQUIRE(static_cast<int>(NodeType::EXPR_COLUMN_REF) == 9);
    REQUIRE(static_cast<int>(NodeType::EXPR_IS_NULL) == 10);

    // Definition types
    REQUIRE(static_cast<int>(NodeType::COLUMN_DEF) == 11);
    REQUIRE(static_cast<int>(NodeType::TABLE_REF) == 12);
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: FAIL with "No source file" or similar

- [ ] **Step 3: Implement NodeType enum in ast.h**

Create `src/parser/ast.h` with the enum:

```cpp
#ifndef SEEDDB_PARSER_AST_H
#define SEEDDB_PARSER_AST_H

#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "parser/token.h"

namespace seeddb {
namespace parser {

/// AST node types
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
    // Definitions
    COLUMN_DEF,
    TABLE_REF
};

/// Data type enumeration
enum class DataType {
    INT,
    BIGINT,
    FLOAT,
    DOUBLE,
    VARCHAR,
    TEXT,
    BOOLEAN
};

/// Data type information with optional length
struct DataTypeInfo {
    DataType base_type_;
    std::optional<size_t> length_;

    DataTypeInfo() : base_type_(DataType::INT) {}
    explicit DataTypeInfo(DataType type) : base_type_(type) {}
    DataTypeInfo(DataType type, size_t len) : base_type_(type), length_(len) {}

    bool has_length() const { return length_.has_value(); }
    size_t length() const { return length_.value_or(0); }
};

/// AST node base class
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual NodeType type() const = 0;
    virtual std::string toString() const = 0;

    Location location() const { return location_; }
    void setLocation(Location loc) { location_ = loc; }

protected:
    Location location_;
};

/// Statement base class
class Stmt : public ASTNode {
};

/// Expression base class
class Expr : public ASTNode {
};

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_AST_H
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: PASS (1 test)

- [ ] **Step 5: Update CMakeLists.txt**

Add ast files to `src/parser/CMakeLists.txt`:

```cmake
# SQL Parser library
add_library(seeddb_parser STATIC
    lexer.cpp
    ast.cpp
)
```

- [ ] **Step 6: Add test file to CMakeLists.txt**

Add to `tests/CMakeLists.txt`:

```cmake
target_sources(seeddb_tests
    PRIVATE
        # ... existing sources ...
        unit/parser/test_ast.cpp
)
```

- [ ] **Step 7: Run full test suite**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure`

Expected: All tests PASS

- [ ] **Step 8: Commit**

```bash
git add src/parser/ast.h src/parser/ast.cpp tests/unit/parser/test_ast.cpp
git add src/parser/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(parser): add AST node type definitions (Phase 2.1)

- Add NodeType enum with statement/expression/definition types
- Add DataType enum and DataTypeInfo struct
- Add ASTNode, Stmt, Expr base classes
- Add unit tests for node types"
```

---

## Chunk 2: Expression Nodes
Implement expression AST nodes with tests.

### Task 2: Implement expression nodes

**Files:**
- Modify: `src/parser/ast.h`
- Modify: `src/parser/ast.cpp`
- Modify: `tests/unit/parser/test_ast.cpp`

- [ ] **Step 1: Write failing tests for LiteralExpr**

Add to `tests/unit/parser/test_ast.cpp`:

```cpp
TEST_CASE("AST: LiteralExpr", "[ast]") {
    SECTION("Integer literal") {
        LiteralExpr expr(TokenValue{int64_t(42)});
        REQUIRE(expr.type() == NodeType::EXPR_LITERAL);
        REQUIRE(std::holds_alternative<int64_t>(expr.value()));
        REQUIRE(std::get<int64_t>(expr.value()) == 42);
        REQUIRE_FALSE(expr.isNull());
    }

    SECTION("String literal") {
        LiteralExpr expr(TokenValue{std::string("hello")});
        REQUIRE(expr.type() == NodeType::EXPR_LITERAL);
        REQUIRE(std::holds_alternative<std::string>(expr.value()));
        REQUIRE(std::get<std::string>(expr.value()) == "hello");
    }

    SECTION("Null literal") {
        LiteralExpr expr(TokenValue{std::monostate{}});
        REQUIRE(expr.type() == NodeType::EXPR_LITERAL);
        REQUIRE(expr.isNull());
    }
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: FAIL (LiteralExpr not defined)

- [ ] **Step 3: Implement LiteralExpr in ast.h**

Add to `src/parser/ast.h`:

```cpp
/// Literal expression (integer, float, string, bool, null)
class LiteralExpr : public Expr {
public:
    explicit LiteralExpr(TokenValue value) : value_(std::move(value)) {}
    LiteralExpr() : value_(std::monostate{}) {}  // Default: NULL

    NodeType type() const override { return NodeType::EXPR_LITERAL; }
    std::string toString() const override;

    const TokenValue& value() const { return value_; }
    bool isNull() const { return std::holds_alternative<std::monostate>(value_); }
    bool isInt() const { return std::holds_alternative<int64_t>(value_); }
    bool isFloat() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }

    int64_t asInt() const { return std::get<int64_t>(value_); }
    double asFloat() const { return std::get<double>(value_); }
    const std::string& asString() const { return std::get<std::string>(value_); }
    bool asBool() const { return std::get<bool>(value_); }

private:
    TokenValue value_;
};
```

- [ ] **Step 4: Implement toString in ast.cpp**

Add to `src/parser/ast.cpp`:

```cpp
std::string LiteralExpr::toString() const {
    if (isNull()) return "NULL";
    if (isInt()) return std::to_string(asInt());
    if (isFloat()) return std::to_string(asFloat());
    if (isBool()) return asBool() ? "TRUE" : "FALSE";
    if (isString()) return "'" + asString() + "'";
    return "UNKNOWN";
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: PASS

- [ ] **Step 6: Write tests for ColumnRef**

Add to test file:

```cpp
TEST_CASE("AST: ColumnRef", "[ast]") {
    SECTION("Simple column reference") {
        ColumnRef ref("name");
        REQUIRE(ref.type() == NodeType::EXPR_COLUMN_REF);
        REQUIRE(ref.column() == "name");
        REQUIRE_FALSE(ref.hasTableQualifier());
        REQUIRE(ref.fullName() == "name");
    }

    SECTION("Qualified column reference") {
        ColumnRef ref("users", "id");
        REQUIRE(ref.hasTableQualifier());
        REQUIRE(ref.table() == "users");
        REQUIRE(ref.column() == "id");
        REQUIRE(ref.fullName() == "users.id");
    }
}
```

- [ ] **Step 7: Implement ColumnRef**

Add to `src/parser/ast.h`:

```cpp
/// Column reference expression
class ColumnRef : public Expr {
public:
    explicit ColumnRef(std::string column) : column_(std::move(column)) {}
    ColumnRef(std::string table, std::string column)
        : table_(std::move(table)), column_(std::move(column)) {}

    NodeType type() const override { return NodeType::EXPR_COLUMN_REF; }
    std::string toString() const override;

    const std::string& table() const { return table_; }
    const std::string& column() const { return column_; }
    bool hasTableQualifier() const { return !table_.empty(); }
    std::string fullName() const {
        return table_.empty() ? column_ : table_ + "." + column_;
    }

private:
    std::string table_;
    std::string column_;
};
```

- [ ] **Step 8: Add toString implementation**

Add to `src/parser/ast.cpp`:

```cpp
std::string ColumnRef::toString() const {
    return fullName();
}
```

- [ ] **Step 9: Run tests and commit**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: PASS

```bash
git add -A
git commit -m "feat(parser): add LiteralExpr and ColumnRef AST nodes

- LiteralExpr: represents integer, float, string, bool, null literals
- ColumnRef: represents column references with optional table qualifier
- Add toString implementations for both classes
- Add comprehensive unit tests"
```

---

## Chunk 3: Binary and Unary Expressions
Implement BinaryExpr and UnaryExpr nodes.

### Task 3: Implement operator expression nodes

**Files:**
- Modify: `src/parser/ast.h`
- Modify: `src/parser/ast.cpp`
- Modify: `tests/unit/parser/test_ast.cpp`

- [ ] **Step 1: Write failing tests for BinaryExpr**

```cpp
TEST_CASE("AST: BinaryExpr", "[ast]") {
    SECTION("Arithmetic expression") {
        auto left = std::make_unique<LiteralExpr>(TokenValue{int64_t(1)});
        auto right = std::make_unique<LiteralExpr>(TokenValue{int64_t(2)});
        BinaryExpr expr("+", std::move(left), std::move(right));

        REQUIRE(expr.type() == NodeType::EXPR_BINARY);
        REQUIRE(expr.op() == "+");
        REQUIRE(expr.left() != nullptr);
        REQUIRE(expr.right() != nullptr);
        REQUIRE(expr.isArithmetic());
        REQUIRE_FALSE(expr.isComparison());
    }
}
```

- [ ] **Step 2: Implement BinaryExpr**

Add to `src/parser/ast.h`:

```cpp
/// Binary expression (arithmetic, comparison, logical)
class BinaryExpr : public Expr {
public:
    BinaryExpr(std::string op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : op_(std::move(op)), left_(std::move(left)), right_(std::move(right)) {}

    NodeType type() const override { return NodeType::EXPR_BINARY; }
    std::string toString() const override;

    const std::string& op() const { return op_; }
    const Expr* left() const { return left_.get(); }
    const Expr* right() const { return right_.get(); }

    bool isArithmetic() const {
        return op_ == "+" || op_ == "-" || op_ == "*" || op_ == "/" || op_ == "%";
    }
    bool isComparison() const {
        return op_ == "=" || op_ == "<>" || op_ == "<" || op_ == ">" ||
               op_ == "<=" || op_ == ">=";
    }
    bool isLogical() const { return op_ == "AND" || op_ == "OR"; }
    bool isConcat() const { return op_ == "||"; }

private:
    std::string op_;
    std::unique_ptr<Expr> left_;
    std::unique_ptr<Expr> right_;
};
```

- [ ] **Step 3: Add toString for BinaryExpr**

Add to `src/parser/ast.cpp`:

```cpp
std::string BinaryExpr::toString() const {
    return "(" + (left_ ? left_->toString() : "null") + " " + op_ + " " +
           (right_ ? right_->toString() : "null") + ")";
}
```

- [ ] **Step 4: Write tests for UnaryExpr**

```cpp
TEST_CASE("AST: UnaryExpr", "[ast]") {
    SECTION("NOT expression") {
        auto operand = std::make_unique<LiteralExpr>(TokenValue{true});
        UnaryExpr expr("NOT", std::move(operand));

        REQUIRE(expr.type() == NodeType::EXPR_UNARY);
        REQUIRE(expr.op() == "NOT");
        REQUIRE(expr.operand() != nullptr);
        REQUIRE(expr.isNot());
        REQUIRE_FALSE(expr.isNegation());
    }
}
```

- [ ] **Step 5: Implement UnaryExpr**

Add to `src/parser/ast.h`:

```cpp
/// Unary expression (NOT, -, +)
class UnaryExpr : public Expr {
public:
    UnaryExpr(std::string op, std::unique_ptr<Expr> operand)
        : op_(std::move(op)), operand_(std::move(operand)) {}

    NodeType type() const override { return NodeType::EXPR_UNARY; }
    std::string toString() const override;

    const std::string& op() const { return op_; }
    const Expr* operand() const { return operand_.get(); }
    bool isNot() const { return op_ == "NOT"; }
    bool isNegation() const { return op_ == "-"; }

private:
    std::string op_;
    std::unique_ptr<Expr> operand_;
};
```

- [ ] **Step 6: Add toString for UnaryExpr**

Add to `src/parser/ast.cpp`:

```cpp
std::string UnaryExpr::toString() const {
    return op_ + "(" + (operand_ ? operand_->toString() : "null") + ")";
}
```

- [ ] **Step 7: Write tests for IsNullExpr**

```cpp
TEST_CASE("AST: IsNullExpr", "[ast]") {
    SECTION("IS NULL") {
        auto expr = std::make_unique<ColumnRef>("email");
        IsNullExpr is_null(std::move(expr), false);

        REQUIRE(is_null.type() == NodeType::EXPR_IS_NULL);
        REQUIRE_FALSE(is_null.isNegated());
    }

    SECTION("IS NOT NULL") {
        auto expr = std::make_unique<ColumnRef>("phone");
        IsNullExpr is_not_null(std::move(expr), true);

        REQUIRE(is_not_null.isNegated());
    }
}
```

- [ ] **Step 8: Implement IsNullExpr**

Add to `src/parser/ast.h`:

```cpp
/// IS NULL expression
class IsNullExpr : public Expr {
public:
    IsNullExpr(std::unique_ptr<Expr> expr, bool negated = false)
        : expr_(std::move(expr)), negated_(negated) {}

    NodeType type() const override { return NodeType::EXPR_IS_NULL; }
    std::string toString() const override;

    const Expr* expr() const { return expr_.get(); }
    bool isNegated() const { return negated_; }

private:
    std::unique_ptr<Expr> expr_;
    bool negated_;
};
```

- [ ] **Step 9: Add toString for IsNullExpr**

```cpp
std::string IsNullExpr::toString() const {
    return (expr_ ? expr_->toString() : "null") + (negated_ ? " IS NOT NULL" : " IS NULL");
}
```

- [ ] **Step 10: Run tests and commit**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: All tests PASS

```bash
git add -A
git commit -m "feat(parser): add BinaryExpr, UnaryExpr, IsNullExpr nodes

- BinaryExpr: supports arithmetic, comparison, logical operators
- UnaryExpr: supports NOT and unary minus
- IsNullExpr: supports IS NULL and IS NOT NULL
- Add toString implementations for all classes
- Add comprehensive unit tests"
```

---

## Chunk 4: Statement Nodes
Implement statement AST nodes (CreateTableStmt, DropTableStmt, etc.)

### Task 4: Implement statement nodes

**Files:**
- Modify: `src/parser/ast.h`
- Modify: `src/parser/ast.cpp`
- Modify: `tests/unit/parser/test_ast.cpp`

- [ ] **Step 1: Write failing tests for CreateTableStmt**

```cpp
TEST_CASE("AST: CreateTableStmt", "[ast]") {
    SECTION("Basic table creation") {
        CreateTableStmt stmt("users");

        REQUIRE(stmt.type() == NodeType::STMT_CREATE_TABLE);
        REQUIRE(stmt.tableName() == "users");
        REQUIRE(stmt.columns().empty());
    }
}
```

- [ ] **Step 2: Implement CreateTableStmt**

Add to `src/parser/ast.h`:

```cpp
/// Column definition
class ColumnDef : public ASTNode {
public:
    ColumnDef(std::string name, DataTypeInfo data_type)
        : name_(std::move(name)), data_type_(std::move(data_type)) {}

    NodeType type() const override { return NodeType::COLUMN_DEF; }
    std::string toString() const override;

    const std::string& name() const { return name_; }
    const DataTypeInfo& dataType() const { return data_type_; }
    bool isNullable() const { return nullable_; }
    void setNullable(bool nullable) { nullable_ = nullable; }

private:
    std::string name_;
    DataTypeInfo data_type_;
    bool nullable_ = true;
};

/// CREATE TABLE statement
class CreateTableStmt : public Stmt {
public:
    explicit CreateTableStmt(std::string table_name) : table_name_(std::move(table_name)) {}

    NodeType type() const override { return NodeType::STMT_CREATE_TABLE; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    const auto& columns() const { return columns_; }

    void addColumn(std::unique_ptr<ColumnDef> col) {
        columns_.push_back(std::move(col));
    }

private:
    std::string table_name_;
    std::vector<std::unique_ptr<ColumnDef>> columns_;
};
```

- [ ] **Step 3: Add toString implementations**

```cpp
std::string ColumnDef::toString() const {
    std::string result = name_ + " " + data_type_to_string(data_type_.base_type_);
    if (data_type_.has_length()) {
        result += "(" + std::to_string(data_type_.length()) + ")";
    }
    if (!nullable_) result += " NOT NULL";
    return result;
}

std::string CreateTableStmt::toString() const {
    std::string result = "CREATE TABLE " + table_name_ + " (";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) result += ", ";
        result += columns_[i]->toString();
    }
    return result + ")";
}

// Helper function
std::string data_type_to_string(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::BIGINT: return "BIGINT";
        case DataType::FLOAT: return "FLOAT";
        case DataType::DOUBLE: return "DOUBLE";
        case DataType::VARCHAR: return "VARCHAR";
        case DataType::TEXT: return "TEXT";
        case DataType::BOOLEAN: return "BOOLEAN";
        default: return "UNKNOWN";
    }
}
```

- [ ] **Step 4: Write tests for DropTableStmt**

```cpp
TEST_CASE("AST: DropTableStmt", "[ast]") {
    SECTION("DROP TABLE") {
        DropTableStmt stmt("old_table");
        REQUIRE(stmt.type() == NodeType::STMT_DROP_TABLE);
        REQUIRE(stmt.tableName() == "old_table");
        REQUIRE_FALSE(stmt.hasIfExists());
    }

    SECTION("DROP TABLE IF EXISTS") {
        DropTableStmt stmt("old_table", true);
        REQUIRE(stmt.hasIfExists());
    }
}
```

- [ ] **Step 5: Implement DropTableStmt**

Add to `src/parser/ast.h`:

```cpp
/// DROP TABLE statement
class DropTableStmt : public Stmt {
public:
    DropTableStmt(std::string table_name, bool if_exists = false)
        : table_name_(std::move(table_name)), if_exists_(if_exists) {}

    NodeType type() const override { return NodeType::STMT_DROP_TABLE; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    bool hasIfExists() const { return if_exists_; }

private:
    std::string table_name_;
    bool if_exists_;
};
```

- [ ] **Step 6: Add toString for DropTableStmt**

```cpp
std::string DropTableStmt::toString() const {
    std::string result = "DROP TABLE ";
    if (if_exists_) result += "IF EXISTS ";
    return result + table_name_;
}
```

- [ ] **Step 7: Write tests for SelectStmt**

```cpp
TEST_CASE("AST: SelectStmt", "[ast]") {
    SECTION("SELECT *") {
        SelectStmt stmt;
        stmt.setSelectAll(true);

        REQUIRE(stmt.type() == NodeType::STMT_SELECT);
        REQUIRE(stmt.isSelectAll());
        REQUIRE_FALSE(stmt.hasWhere());
    }
}
```

- [ ] **Step 8: Implement SelectStmt**

Add to `src/parser/ast.h`:

```cpp
/// Table reference
class TableRef : public ASTNode {
public:
    explicit TableRef(std::string name) : name_(std::move(name)) {}
    TableRef(std::string name, std::string alias)
        : name_(std::move(name)), alias_(std::move(alias)) {}

    NodeType type() const override { return NodeType::TABLE_REF; }
    std::string toString() const override;

    const std::string& name() const { return name_; }
    const std::string& alias() const { return alias_; }
    bool hasAlias() const { return !alias_.empty(); }

private:
    std::string name_;
    std::string alias_;
};

/// SELECT statement
class SelectStmt : public Stmt {
public:
    NodeType type() const override { return NodeType::STMT_SELECT; }
    std::string toString() const override;

    bool isSelectAll() const { return select_all_; }
    const auto& columns() const { return columns_; }
    const TableRef* fromTable() const { return from_table_.get(); }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }

    void setSelectAll(bool all) { select_all_ = all; }
    void addColumn(std::unique_ptr<Expr> col) { columns_.push_back(std::move(col)); }
    void setFromTable(std::unique_ptr<TableRef> table) { from_table_ = std::move(table); }
    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }

private:
    bool select_all_ = false;
    std::vector<std::unique_ptr<Expr>> columns_;
    std::unique_ptr<TableRef> from_table_;
    std::unique_ptr<Expr> where_clause_;
};
```

- [ ] **Step 9: Add toString for SelectStmt and TableRef**

```cpp
std::string TableRef::toString() const {
    if (hasAlias()) return name_ + " AS " + alias_;
    return name_;
}

 std::string SelectStmt::toString() const {
    std::string result = "SELECT ";
    if (select_all_) {
        result += "*";
    } else {
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) result += ", ";
            result += columns_[i]->toString();
        }
    }
    if (from_table_) {
        result += " FROM " + from_table_->toString();
    }
    if (where_clause_) {
        result += " WHERE " + where_clause_->toString();
    }
    return result;
}
```

- [ ] **Step 10: Write tests for InsertStmt, UpdateStmt, DeleteStmt**

Add tests for remaining statements (similar pattern to above).

- [ ] **Step 11: Implement remaining statement classes**

Implement InsertStmt, UpdateStmt, DeleteStmt following the same pattern as above.

- [ ] **Step 12: Add toString implementations**

Add toString for each statement class.

- [ ] **Step 13: Run tests and commit**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_ast`

Expected: All tests PASS

```bash
git add -A
git commit -m "feat(parser): implement statement AST nodes

- CreateTableStmt: represents CREATE TABLE with columns
- DropTableStmt: represents DROP TABLE [IF EXISTS]
- SelectStmt: represents SELECT with columns, FROM, WHERE
- InsertStmt: represents INSERT with columns and values
- UpdateStmt: represents UPDATE with SET and WHERE
- DeleteStmt: represents DELETE with WHERE
- TableRef: represents table reference with optional alias
- ColumnDef: represents column definition
- Add toString implementations and comprehensive unit tests"
```

---

## Chunk 5: Parser Class Foundation
Create parser.h with basic infrastructure and token manipulation methods.

### Task 5: Implement parser infrastructure

**Files:**
- Create: `src/parser/parser.h`
- Create: `src/parser/parser.cpp`
- Create: `tests/unit/parser/test_parser.cpp`
- Modify: `src/parser/CMakeLists.txt`
- modify: `tests/CMakeLists.txt`
- Update: `docs/superpowers/progress.md`

 (Phase 2: 100% complete)

 **Steps:**

- [ ] **Step 1: Write failing tests for parser construction**

Create `tests/unit/parser/test_parser.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "parser/parser.h"
#include "parser/lexer.h"

using namespace seeddb::parser;

TEST_CASE("Parser: construction", "[parser]") {
    Lexer lexer("SELECT 1");
    Parser parser(lexer);

    REQUIRE(parser.has_more());  // Should have tokens available
}

 REQUIRE(parser.current().type == TokenType::SELECT);
    }
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_parser`

Expected: FAIL (Parser class doesn't exist)

- [ ] **Step 3: Implement parser.h**

Create `src/parser/parser.h`:

```cpp
#ifndef SEEDDB_PARSER_PARSER_H
#define SEEDDB_PARSER_PARSER_H

#include <memory>
#include "parser/lexer.h"
#include "parser/ast.h"

namespace seeddb {
namespace parser {

/// SQL parser - converts token stream to AST
class Parser {
public:
    /// Construct parser from lexer (takes reference)
    explicit Parser(Lexer& lexer);

    /// Parse single SQL statement
    Result<std::unique_ptr<Stmt>> parse();

    /// Parse multiple SQL statements (semicolon-separated)
    Result<std::vector<std::unique_ptr<Stmt>>> parseAll();

    /// Check if there are more tokens
    bool has_more() const;

    /// Get current token (lookahead, without consuming)
    const Token& current() const;

    /// Get current token type
    TokenType currentType() const { return current_token_.type; }

    /// Consume and return current token
    Token consume();

    /// Expect specific token type,    Result<Token> expect(TokenType type, const char* message);
    /// Check if current token matches type
    bool check(TokenType type) const;
    /// Match and consume if type matches
    bool match(TokenType type);

private:
    Lexer& lexer_;
    Token current_token_;
    std::string current_keyword_;
};

```

- [ ] **Step 4: Implement parser.cpp skeleton**

Create `src/parser/parser.cpp`:

```cpp
#include "parser/parser.h"
#include "parser/ast.h"
#include "common/error.h"

#include <sstream>

namespace seeddb {
namespace parser {

Parser::Parser(Lexer& lexer) : lexer_(lexer) {
    // Initialize current token
    auto first = lexer.peek_token();
    if (first.is_ok()) {
        current_token_ = first.value();
    } else {
        current_token_ = {TokenType::END_OF_INPUT, {}, std::monostate{}};
        }
    }
}

Parser::~Parser() = default;

}

bool Parser::has_more() const {
    return lexer_.has_more();
    }

const Token& Parser::current() const {
    return current_token_;
        }

TokenType Parser::currentType() const {
            return current_token_.type;
        }

Token Parser::consume() {
            auto old = current_token_;
            if (lexer_.peek_token().is_ok()) {
                current_token_ = lexer_.next_token();
                // Update current_keyword_
                current_keyword_ = extractKeywordString(old.value);
                : old.value;
                : std::get<std::string>(old.value);
            }
            return "";
        }
        return "";
    }
        return TokenType::END_OF_INPUT;
    }

Result<Token> Parser::expect(TokenType type, const char* message) {
    auto result = lexer_.peek_token();
    if (!result.is_ok()) {
        return Result<Token>::err(ErrorCode::SYNTAX_ERROR,
 "Expected " + std::string(message));
    }
    auto tok = result.value;
    current_token_ = tok;
    return Result<Token>::ok(std_token_);
}

 }
        return Result<Token>::err(ErrorCode::SYNTAX_ERROR,
 "Expected " + std::string(message));
    }
}

Token Parser::check(TokenType type) const {
    return current_token_.type == type;
}

```

- [ ] **Step 5: Update CMakeLists.txt**

Add to `src/parser/CMakeLists.txt`:

```cmake
# SQL Parser library
add_library(seeddb_parser STATIC
    lexer.cpp
    parser.cpp
    ast.cpp
)
```

Add to `tests/CMakeLists.txt`:

```cmake
        unit/parser/test_parser.cpp
```

- [ ] **Step 6: Run tests and commit**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure`

Expected: PASS (parser construction test)

```bash
git add src/parser/parser.h src/parser/parser.cpp tests/unit/parser/test_parser.cpp
git add src/parser/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(parser): add parser class infrastructure

- Parser class with Lexer reference
- Token lookahead (current_token_, has_more())
- Token manipulation: consume(), expect(), check()
- Add parser to CMakeLists and
- Add test file to CMakeLists
"
```

---

## Chunk 6: DDL Statement Parsing
Implement CREATE TABLE and DROP table parsing

### Task 6: Implement DDL parsing

**Files:**
- Modify: `src/parser/parser.cpp`
- Modify: `tests/unit/parser/test_parser.cpp`

- [ ] **Step 1: Write failing test for CREATE TABLE**

Add to test file:

```cpp
TEST_CASE("Parser: CREATE TABLE", "[parser]") {
    SECTION("Basic CREATE TABLE") {
        std::string sql = "CREATE TABLE users (id INT, name TEXT)";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_CREATE_TABLE);

        auto* create = static_cast<CreateTableStmt*>(stmt.get());
        REQUIRE(create->tableName() == "users");
        REQUIRE(create->columns().size() == 2);
    }
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_parser`

Expected: FAIL (parseCreateTable not implemented)

- [ ] **Step 3: Implement parseCreateTable**

Add to `src/parser/parser.cpp`:

```cpp
Result<std::unique_ptr<Stmt>> Parser::parseStatement() {
    switch (current_token_.type) {
        case TokenType::CREATE:
            return parseCreateTable();
        case TokenType::DROP:
            return parseDropTable();
        case TokenType::SELECT:
            return parseSelect();
        case TokenType::INSERT:
            return parseInsert();
        case TokenType::UPDATE:
            return parseUpdate();
        case TokenType::DELETE:
            return parseDelete();
        default:
            return syntax_error("Unexpected token: " + token_type_name(current_token_.type));
    }
}

Result<std::unique_ptr<CreateTableStmt>> Parser::parseCreateTable() {
    // Expect CREATE
    auto tok = consume();
    if (tok.type != TokenType::CREATE) {
        return syntax_error("Expected CREATE");
    }

    // Expect TABLE
    tok = consume();
    if (tok.type != TokenType::TABLE) {
        return syntax_error("Expected TABLE");
    }

    // Get table name
    tok = consume();
    if (tok.type != TokenType::IDENTIFIER) {
        return syntax_error("Expected table name");
    }
    auto table_name = std::get<std::string>(tok.value);

    consume();  // consume TABLE

    // Expect (
    if (!check(TokenType::LPAREN)) {
        return syntax_error("Expected '('");
    }
    consume();

    // Parse column definitions
    auto columns = parseColumnDefList();
    if (!columns.is_ok()) {
        return Result<std::vector<std::unique_ptr<ColumnDef>>>::err(columns.error());
    }

    // Expect )
    if (!check(TokenType::RPAREN)) {
        return syntax_error("Expected ')");
    }
    consume();

    auto stmt = std::make_unique<CreateTableStmt>(std::move(table_name));
    for (auto& col : columns.value()) {
        stmt->addColumn(std::move(col));
    }
    return Result<std::unique_ptr<CreateTableStmt>>::ok(std_cast(stmt));
    }

    return syntax_error("Expected ')' + token_type_name(current_token_.type));
}
```

- [ ] **Step 4: Implement helper methods for column parsing**

Add helper methods for `src/parser/parser.cpp`:

```cpp
Result<std::vector<std::unique_ptr<ColumnDef>>> Parser::parseColumnDefList() {
    std::vector<std::unique_ptr<ColumnDef>> columns;

    while (check(TokenType::RPAREN)) {
        auto col = parseColumnDef();
        if (!col.is_ok()) {
            return Result<std::vector<std::unique_ptr<ColumnDef>>>::err(col.error());
        }
        columns.push_back(std::move(col.value()));

        if (match(TokenType::COMMA)) {
            consume();
            continue;
        }
        break;
    }

    return Result<std::vector<std::unique_ptr<ColumnDef>>>::ok(std::move(columns));
}

Result<std::unique_ptr<ColumnDef>> Parser::parseColumnDef() {
    // Column name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error("Expected column name");
    }
    std::string name = std::get<std::string>(current_token_.value);
    consume();

    // Data type
    auto type_result = parseDataType();
    if (!type_result.is_ok()) {
        return Result<std::unique_ptr<ColumnDef>>::err(type_result.error());
    }

    auto col = std::make_unique<ColumnDef>(std::move(name), type_result.value());

    // Optional NOT NULL
    if (match(TokenType::NOT)) {
        if (!match(TokenType::NULL_LIT)) {
            return syntax_error("Expected NULL after NOT");
        }
        col->setNullable(false);
    }

    return Result<std::unique_ptr<ColumnDef>>::ok(std::move(col));
}

Result<DataTypeInfo> Parser::parseDataType() {
    // Map token types to data types
    DataType type;
    switch (current_token_.type) {
        case TokenType::INTEGER:
        case TokenType::INT:
            type = DataType::INT;
            break;
        case TokenType::BIGINT:
            type = DataType::BIGINT;
            break;
        case TokenType::FLOAT:
            type = DataType::FLOAT;
            break;
        case TokenType::DOUBLE:
            type = DataType::DOUBLE;
            break;
        case TokenType::VARCHAR: {
            type = DataType::VARCHAR;
            size_t length = 0;
            if (match(TokenType::LPAREN)) {
                if (!check(TokenType::INTEGER_LIT)) {
                    return syntax_error("Expected VARCHAR length");
                }
                length = static_cast<size_t>(std::get<int64_t>(current_token_.value));
                consume();
                if (!match(TokenType::RPAREN)) {
                    return syntax_error("Expected ')'");
                }
            }
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type, length));
        }
        case TokenType::TEXT:
            type = DataType::TEXT;
            break;
        case TokenType::BOOLEAN:
        case TokenType::BOOL:
            type = DataType::BOOLEAN;
            break;
        default:
            return syntax_error("Expected data type");
    }
    consume();
    return Result<DataTypeInfo>::ok(DataTypeInfo(type));
}
```

- [ ] **Step 5: Write test for DROP TABLE**

```cpp
TEST_CASE("Parser: DROP TABLE", "[parser]") {
    SECTION("Basic DROP TABLE") {
        std::string sql = "DROP TABLE users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_DROP_TABLE);

        auto* drop = static_cast<DropTableStmt*>(stmt.get());
        REQUIRE(drop->tableName() == "users");
        REQUIRE_FALSE(drop->hasIfExists());
    }

    SECTION("DROP TABLE IF EXISTS") {
        std::string sql = "DROP TABLE IF EXISTS old_table";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* drop = static_cast<DropTableStmt*>(result.value().get());
        REQUIRE(drop->hasIfExists());
    }
}
```

- [ ] **Step 6: Implement parseDropTable**

```cpp
Result<std::unique_ptr<DropTableStmt>> Parser::parseDropTable() {
    consume();  // DROP

    if (!check(TokenType::TABLE)) {
        return syntax_error("Expected TABLE");
    }
    consume();

    bool if_exists = false;
    if (match(TokenType::IF)) {
        if (!match(TokenType::EXISTS)) {
            return syntax_error("Expected EXISTS after IF");
        }
        if_exists = true;
    }

    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error("Expected table name");
    }
    std::string name = std::get<std::string>(current_token_.value);
    consume();

    return Result<std::unique_ptr<DropTableStmt>>::ok(
        std::make_unique<DropTableStmt>(std::move(name), if_exists)
    );
}
```

- [ ] **Step 7: Run tests and commit**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure -R test_parser`

Expected: DDL tests PASS

```bash
git add -A
git commit -m "feat(parser): implement DDL statement parsing

- parseCreateTable: parse CREATE TABLE with column definitions
- parseDropTable: parse DROP TABLE [IF EXISTS]
- parseColumnDefList: parse comma-separated column definitions
- parseDataType: map token types to DataType enum
- Add unit tests for both statement types"
```

---

## Chunk 7: DML Statement parsing
Implement INSERT, SELECT, UPDATE, DELETE parsing

### Task 7: Implement DML parsing

**Files:**
- Modify: `src/parser/parser.cpp`
- Modify: `tests/unit/parser/test_parser.cpp`

- [ ] **Step 1: Write failing tests for SELECT**

```cpp
TEST_CASE("Parser: SELECT", "[parser]") {
    SECTION("SELECT *") {
        std::string sql = "SELECT * FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_SELECT);

        auto* select = static_cast<SelectStmt*>(stmt.get());
        REQUIRE(select->isSelectAll());
        REQUIRE(select->fromTable()->name() == "users");
    }

    SECTION("SELECT with WHERE") {
        std::string sql = "SELECT id, name FROM users WHERE age > 18";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE_FALSE(select->isSelectAll());
        REQUIRE(select->columns().size() == 2);
        REQUIRE(select->hasWhere());
    }
}
```

- [ ] **Step 2: Implement parseSelect**

```cpp
Result<std::unique_ptr<SelectStmt>> Parser::parseSelect() {
    consume();  // SELECT

    auto stmt = std::make_unique<SelectStmt>();

    // Parse columns
    if (check(TokenType::STAR)) {
        stmt->setSelectAll(true);
        consume();
    } else {
        // Parse expression list
        do {
            auto expr = parseExpression();
            if (!expr.is_ok()) {
                return Result<std::unique_ptr<SelectStmt>>::err(expr.error());
            }
            stmt->addColumn(std::move(expr.value()));
        } while (match(TokenType::COMMA));
    }

    // FROM clause
    if (!match(TokenType::FROM)) {
        return syntax_error("Expected FROM");
    }

    auto table = parseTableRef();
    if (!table.is_ok()) {
        return Result<std::unique_ptr<SelectStmt>>::err(table.error());
    }
    stmt->setFromTable(std::move(table.value()));

    // Optional WHERE
    if (match(TokenType::WHERE)) {
        auto where = parseExpression();
        if (!where.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(where.error());
        }
        stmt->setWhere(std::move(where.value()));
    }

    return Result<std::unique_ptr<SelectStmt>>::ok(std::move(stmt));
}
```

- [ ] **Step 3: Implement parseExpression (basic version)**

Add minimal expression parsing for initial tests:

```cpp
Result<std::unique_ptr<Expr>> Parser::parseExpression() {
    return parseOrExpr();
}

Result<std::unique_ptr<Expr>> Parser::parseOrExpr() {
    auto left = parseAndExpr();
    if (!left.is_ok()) return left;

    while (match(TokenType::OR)) {
        auto right = parseAndExpr();
        if (!right.is_ok()) return right;

        left = Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>("OR", std::move(left.value()), std::move(right.value()))
        );
    }
    return left;
}
```

- [ ] **Step 4: Implement remaining expression methods**

Implement parseAndExpr, parseNotExpr, parseComparisonExpr, parseAdditiveExpr, parseMultiplicativeExpr, parseUnaryExpr, parsePrimaryExpr following the operator precedence in the spec

- [ ] **Step 5: Write tests for INSERT**

```cpp
TEST_CASE("Parser: INSERT", "[parser]") {
    SECTION("INSERT with values") {
        std::string sql = "INSERT INTO users (id, name) VALUES (1, 'Alice')";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_INSERT);

        auto* insert = static_cast<InsertStmt*>(stmt.get());
        REQUIRE(insert->tableName() == "users");
        REQUIRE(insert->hasExplicitColumns());
        REQUIRE(insert->values().size() == 2);
    }
}
```

- [ ] **Step 6: Implement parseInsert**

```cpp
Result<std::unique_ptr<InsertStmt>> Parser::parseInsert() {
    consume();  // INSERT

    if (!match(TokenType::INTO)) {
        return syntax_error("Expected INTO");
    }

    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error("Expected table name");
    }
    auto stmt = std::make_unique<InsertStmt>(std::get<std::string>(current_token_.value));
    consume();

    // Optional column list
    if (match(TokenType::LPAREN)) {
        do {
            if (!check(TokenType::IDENTIFIER)) {
                return syntax_error("Expected column name");
            }
            stmt->addColumn(std::get<std::string>(current_token_.value));
            consume();
        } while (match(TokenType::COMMA));

        if (!match(TokenType::RPAREN)) {
            return syntax_error("Expected ')'");
        }
    }
    }

    // VALUES
    if (!match(TokenType::VALUES)) {
        return syntax_error("Expected VALUES");
    }

    if (!match(TokenType::LPAREN)) {
        return syntax_error("Expected '('");
    }

    do {
        auto val = parseExpression();
        if (!val.is_ok()) {
            return Result<std::unique_ptr<InsertStmt>>::err(val.error());
        }
        stmt->addValue(std::move(val.value()));
    } while (match(TokenType::COMMA));

    if (!match(TokenType::RPAREN)) {
        return syntax_error("Expected ')'");
    }

    return Result<std::unique_ptr<InsertStmt>>::ok(std::move(stmt));
    }
```

- [ ] **Step 7: Write tests for UPDATE and DELETE**

Add tests for UPDATE and DELETE statements

- [ ] **Step 8: Implement parseUpdate and parseDelete**

Implement remaining DML statement parsers

- [ ] **Step 9: Run tests and commit**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure`

Expected: All DML tests PASS

```bash
git add -A
git commit -m "feat(parser): implement DML statement parsing

- parseSelect: parse SELECT with columns, FROM, WHERE
- parseInsert: parse INSERT with columns and values
- parseUpdate: parse UPDATE with SET and WHERE
- parseDelete: parse DELETE with WHERE
- parseExpression: implement expression parsing with operator precedence
- Add comprehensive unit tests for all statement types"
```

---

## Chunk 8: Error Handling and Documentation
Final testing and documentation updates

### Task 8: Finalize implementation

**Files:**
- Modify: `tests/unit/parser/test_parser.cpp`
- Update: `docs/superpowers/progress.md`
- Update: `CLimb` memory (optional)
`CLimb` memory

 (system reminder about memory limits)

- [ ] **Step 1: Write error handling tests**

```cpp
TEST_CASE("Parser: Error handling", "[parser]") {
    SECTION("Syntax error location") {
        std::string sql = "SELECT * FORM";  // Missing table name
 Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }
}
```

- [ ] **Step 2: Verify error messages are descriptive**

Run tests to verify error messages include location information

- [ ] **Step 3: Update progress documentation**

Update `docs/superpowers/progress.md`:

```markdown
## Phase 2: SQL Parser - ✅ COMPLETE

```

**Status:** Complete
**end date:** 2026-03-16
**duration:** 6 days (estimated)

++ hours)

**Goals:**
- [x] Implement complete SQL parser with AST generation
- [x] Support DDL (CREATE TABLE, DROP TABLE) and DML (SELECT, INSERT, UPDATE, DELETE)
 statements
- [x] Hand-written recursive descent parser with zero external dependencies
- [x] Comprehensive test coverage
- [x] Proper error handling with location information

 **deliverables:**
- `src/parser/ast.h` - AST node definitions
- `src/parser/ast.cpp` - AST toString implementations
- `src/parser/parser.h` - Parser interface
- `src/parser/parser.cpp` - Parser implementation
- `tests/unit/parser/test_ast.cpp` - AST unit tests
- `tests/unit/parser/test_parser.cpp` - Parser unit tests

```

- [ ] **Step 4: Run final verification**

Run: `cd build && make -j$(nproc) && ctest --output-on-failure`

Expected: ALL tests PASS (100%+)

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat(parser): complete SQL parser implementation (Phase 2)

- Implement full parser with AST generation
- Support DDL: CREATE TABLE, DROP TABLE
 IF EXISTS

 Support DML: SELECT, INSERT, UPDATE, DELETE
 statements
- Expression parsing with operator precedence
- Comprehensive error handling with location info
- Complete test coverage
- Update progress documentation

Co-authored-by: Claude Opus 4.6 <noreply@anthropic.com>
```

---

Plan complete and saved to `docs/superpowers/plans/2026-03-16-sql-parser-design.md`. Ready to execute?