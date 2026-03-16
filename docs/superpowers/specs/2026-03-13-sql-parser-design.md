# SeedDB SQL Parser 设计规范

**日期**: 2026-03-13
**阶段**: Phase 2
**作者**: Claude

## 1. 概述

为 SeedDB 实现 SQL 语法分析器（Parser），将词法分析器（Lexer）产生的 Token 流转换为抽象语法树（AST）。本设计遵循 PostgreSQL 语法子集，采用手写递归下降解析方法。

## 2. 需求

### 2.1 功能范围

| 类别 | 支持的语句 |
|------|-----------|
| **DDL** | `CREATE TABLE`, `DROP TABLE` |
| **DML** | `SELECT`, `INSERT`, `UPDATE`, `DELETE` |

### 2.2 表达式支持

- **算术运算**: `+`, `-`, `*`, `/`, `%`, `||` (字符串拼接)
- **比较运算**: `=`, `<>`, `<`, `>`, `<=`, `>=`
- **逻辑运算**: `AND`, `OR`, `NOT`
- **特殊运算**: `IS NULL`, `IS NOT NULL`
- **字面量**: 整数、浮点数、字符串、布尔值、NULL

### 2.3 设计约束

- **解析方法**: 手写递归下降（零依赖）
- **错误处理**: 快速失败，返回详细错误位置
- **内存管理**: 使用 `std::unique_ptr` 管理 AST 节点

## 3. 架构设计

### 3.1 数据流

```
SQL 字符串
    │
    ▼
┌─────────────┐
│   Lexer     │  词法分析 (Phase 1 ✅)
└─────┬───────┘
      │ Token 流
      ▼
┌─────────────┐
│   Parser    │  语法分析 (Phase 2 🆕)
└─────┬───────┘
      │ AST
      ▼
┌─────────────┐
│  Executor   │  查询执行 (Phase 4)
└─────────────┘
```

### 3.2 文件结构

```
src/parser/
├── lexer.h/cpp        # ✅ Phase 1 已完成
├── token.h            # ✅ Phase 1 已完成
├── keywords.h         # ✅ Phase 1 已完成
├── ast.h              # 🆕 AST 节点定义
├── ast.cpp            # 🆕 AST toString 实现
├── parser.h           # 🆕 Parser 类接口
└── parser.cpp         # 🆕 Parser 类实现
```

## 4. AST 节点设计

### 4.1 类型层次

采用类继承层次设计，清晰直观，便于扩展。

```
ASTNode (基类)
│
├── Stmt (语句基类)
│   ├── CreateTableStmt
│   ├── DropTableStmt
│   ├── InsertStmt
│   ├── SelectStmt
│   ├── UpdateStmt
│   └── DeleteStmt
│
├── Expr (表达式基类)
│   ├── BinaryExpr
│   ├── UnaryExpr
│   ├── LiteralExpr
│   ├── ColumnRef
│   └── IsNullExpr
│
└── Definition
    ├── ColumnDef
    └── TableRef
```

### 4.2 节点类型枚举

```cpp
enum class NodeType {
    // 语句
    STMT_CREATE_TABLE,
    STMT_DROP_TABLE,
    STMT_INSERT,
    STMT_SELECT,
    STMT_UPDATE,
    STMT_DELETE,
    // 表达式
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_COLUMN_REF,
    EXPR_IS_NULL,
    // 定义
    COLUMN_DEF,
    TABLE_REF,
};
```

### 4.3 基类定义

```cpp
/// AST 节点基类
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

/// 语句基类
class Stmt : public ASTNode {};

/// 表达式基类
class Expr : public ASTNode {};
```

### 4.4 辅助类型定义

在详细定义各节点之前，先定义一些辅助类型：

```cpp
/// 数据类型枚举（用于 ColumnDef）
enum class DataType {
    INT,
    BIGINT,
    FLOAT,
    DOUBLE,
    VARCHAR,
    TEXT,
    BOOLEAN,
};

/// 数据类型信息（包含可选的长度参数）
struct DataTypeInfo {
    DataType base_type;
    std::optional<size_t> length;    // VARCHAR(n) 中的 n
};
```

### 4.5 语句节点详细定义

#### CreateTableStmt - 创建表语句

**SQL 语法**:
```sql
CREATE TABLE table_name (
    column_name data_type [NOT NULL] [DEFAULT value],
    ...
)
```

**类定义**:
```cpp
class CreateTableStmt : public Stmt {
public:
    // ========== 核心数据成员 ==========
    std::string table_name_;                            // 表名
    std::vector<std::unique_ptr<ColumnDef>> columns_;   // 列定义列表

    // ========== 构造函数 ==========
    CreateTableStmt() = default;
    explicit CreateTableStmt(std::string table_name)
        : table_name_(std::move(table_name)) {}

    // ========== 访问器 ==========
    const std::string& tableName() const { return table_name_; }
    const auto& columns() const { return columns_; }

    void addColumn(std::unique_ptr<ColumnDef> col) {
        columns_.push_back(std::move(col));
    }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::STMT_CREATE_TABLE; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: CREATE TABLE users (id INT NOT NULL, name TEXT)

CreateTableStmt
├── table_name_: "users"
└── columns_:
    ├── ColumnDef { name: "id", data_type: INT, nullable: false }
    └── ColumnDef { name: "name", data_type: TEXT, nullable: true }
```

---

#### DropTableStmt - 删除表语句

**SQL 语法**:
```sql
DROP TABLE [IF EXISTS] table_name
```

**类定义**:
```cpp
class DropTableStmt : public Stmt {
public:
    // ========== 核心数据成员 ==========
    std::string table_name_;    // 表名
    bool if_exists_ = false;    // 是否有 IF EXISTS 子句

    // ========== 构造函数 ==========
    DropTableStmt() = default;
    DropTableStmt(std::string table_name, bool if_exists = false)
        : table_name_(std::move(table_name)), if_exists_(if_exists) {}

    // ========== 访问器 ==========
    const std::string& tableName() const { return table_name_; }
    bool hasIfExists() const { return if_exists_; }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::STMT_DROP_TABLE; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: DROP TABLE IF EXISTS old_table

DropTableStmt
├── table_name_: "old_table"
└── if_exists_: true
```

---

#### InsertStmt - 插入语句

**SQL 语法**:
```sql
INSERT INTO table_name [(column1, column2, ...)] VALUES (value1, value2, ...)
```

**类定义**:
```cpp
class InsertStmt : public Stmt {
public:
    // ========== 核心数据成员 ==========
    std::string table_name_;                            // 目标表名
    std::vector<std::string> columns_;                  // 可选：指定列名
    std::vector<std::unique_ptr<Expr>> values_;         // VALUES 列表

    // ========== 构造函数 ==========
    InsertStmt() = default;
    explicit InsertStmt(std::string table_name)
        : table_name_(std::move(table_name)) {}

    // ========== 访问器 ==========
    const std::string& tableName() const { return table_name_; }
    const auto& columns() const { return columns_; }
    const auto& values() const { return values_; }
    bool hasExplicitColumns() const { return !columns_.empty(); }

    // ========== 修改器 ==========
    void addColumn(std::string col) { columns_.push_back(std::move(col)); }
    void addValue(std::unique_ptr<Expr> val) { values_.push_back(std::move(val)); }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::STMT_INSERT; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: INSERT INTO users (id, name) VALUES (1, 'Alice')

InsertStmt
├── table_name_: "users"
├── columns_: ["id", "name"]
└── values_:
    ├── LiteralExpr { value: int64(1) }
    └── LiteralExpr { value: string("Alice") }
```

---

#### SelectStmt - 查询语句

**SQL 语法**:
```sql
SELECT [* | expr1, expr2, ...] FROM table_name [WHERE condition]
```

**类定义**:
```cpp
class SelectStmt : public Stmt {
public:
    // ========== 核心数据成员 ==========
    bool select_all_ = false;                           // SELECT *
    std::vector<std::unique_ptr<Expr>> columns_;        // 选择列（非 * 时）
    std::unique_ptr<TableRef> from_table_;              // FROM 子句
    std::unique_ptr<Expr> where_clause_;                // WHERE 子句（可空）

    // ========== 构造函数 ==========
    SelectStmt() = default;

    // ========== 访问器 ==========
    bool isSelectAll() const { return select_all_; }
    const auto& columns() const { return columns_; }
    const TableRef* fromTable() const { return from_table_.get(); }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }

    // ========== 修改器 ==========
    void setSelectAll(bool all) { select_all_ = all; }
    void addColumn(std::unique_ptr<Expr> col) { columns_.push_back(std::move(col)); }
    void setFromTable(std::unique_ptr<TableRef> table) { from_table_ = std::move(table); }
    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::STMT_SELECT; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: SELECT id, name FROM users WHERE age > 18

SelectStmt
├── select_all_: false
├── columns_:
│   ├── ColumnRef { column: "id" }
│   └── ColumnRef { column: "name" }
├── from_table_: TableRef { name: "users" }
└── where_clause_:
    └── BinaryExpr { op: ">", left: ColumnRef("age"), right: LiteralExpr(18) }
```

---

#### UpdateStmt - 更新语句

**SQL 语法**:
```sql
UPDATE table_name SET column1 = value1, column2 = value2, ... [WHERE condition]
```

**类定义**:
```cpp
/// SET 子句中的单个赋值
struct Assignment {
    std::string column_;                // 列名
    std::unique_ptr<Expr> value_;       // 新值表达式

    Assignment() = default;
    Assignment(std::string col, std::unique_ptr<Expr> val)
        : column_(std::move(col)), value_(std::move(val)) {}
};

class UpdateStmt : public Stmt {
public:
    // ========== 核心数据成员 ==========
    std::string table_name_;                        // 目标表名
    std::vector<Assignment> assignments_;           // SET 子句
    std::unique_ptr<Expr> where_clause_;            // WHERE 子句（可空）

    // ========== 构造函数 ==========
    UpdateStmt() = default;
    explicit UpdateStmt(std::string table_name)
        : table_name_(std::move(table_name)) {}

    // ========== 访问器 ==========
    const std::string& tableName() const { return table_name_; }
    const auto& assignments() const { return assignments_; }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }

    // ========== 修改器 ==========
    void addAssignment(std::string col, std::unique_ptr<Expr> val) {
        assignments_.emplace_back(std::move(col), std::move(val));
    }
    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::STMT_UPDATE; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: UPDATE users SET age = 25, status = 'active' WHERE id = 1

UpdateStmt
├── table_name_: "users"
├── assignments_:
│   ├── Assignment { column: "age", value: LiteralExpr(25) }
│   └── Assignment { column: "status", value: LiteralExpr("active") }
└── where_clause_:
    └── BinaryExpr { op: "=", left: ColumnRef("id"), right: LiteralExpr(1) }
```

---

#### DeleteStmt - 删除语句

**SQL 语法**:
```sql
DELETE FROM table_name [WHERE condition]
```

**类定义**:
```cpp
class DeleteStmt : public Stmt {
public:
    // ========== 核心数据成员 ==========
    std::string table_name_;                // 目标表名
    std::unique_ptr<Expr> where_clause_;    // WHERE 子句（可空）

    // ========== 构造函数 ==========
    DeleteStmt() = default;
    explicit DeleteStmt(std::string table_name)
        : table_name_(std::move(table_name)) {}

    // ========== 访问器 ==========
    const std::string& tableName() const { return table_name_; }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }

    // ========== 修改器 ==========
    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::STMT_DELETE; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: DELETE FROM users WHERE status = 'inactive'

DeleteStmt
├── table_name_: "users"
└── where_clause_:
    └── BinaryExpr { op: "=", left: ColumnRef("status"), right: LiteralExpr("inactive") }
```

### 4.6 表达式节点详细定义

#### BinaryExpr - 二元运算表达式

**支持的运算符**:
| 类别 | 运算符 | 示例 |
|------|--------|------|
| 算术 | `+`, `-`, `*`, `/`, `%` | `a + b`, `x * 2` |
| 字符串 | `||` | `'Hello' || 'World'` |
| 比较 | `=`, `<>`, `<`, `>`, `<=`, `>=` | `a > 10`, `x <> y` |
| 逻辑 | `AND`, `OR` | `a > 0 AND b < 10` |

**类定义**:
```cpp
class BinaryExpr : public Expr {
public:
    // ========== 核心数据成员 ==========
    std::string op_;                         // 运算符字符串
    std::unique_ptr<Expr> left_;             // 左操作数
    std::unique_ptr<Expr> right_;            // 右操作数

    // ========== 构造函数 ==========
    BinaryExpr() = default;
    BinaryExpr(std::string op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : op_(std::move(op)), left_(std::move(left)), right_(std::move(right)) {}

    // ========== 访问器 ==========
    const std::string& op() const { return op_; }
    const Expr* left() const { return left_.get(); }
    const Expr* right() const { return right_.get(); }

    // ========== 运算符分类 ==========
    bool isArithmetic() const {
        return op_ == "+" || op_ == "-" || op_ == "*" || op_ == "/" || op_ == "%";
    }
    bool isComparison() const {
        return op_ == "=" || op_ == "<>" || op_ == "<" || op_ == ">" ||
               op_ == "<=" || op_ == ">=";
    }
    bool isLogical() const { return op_ == "AND" || op_ == "OR"; }
    bool isConcat() const { return op_ == "||"; }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::EXPR_BINARY; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: a + b * c

BinaryExpr { op: "+" }
├── left_: ColumnRef { column: "a" }
└── right_: BinaryExpr { op: "*" }
            ├── left_: ColumnRef { column: "b" }
            └── right_: ColumnRef { column: "c" }
```

---

#### UnaryExpr - 一元运算表达式

**支持的运算符**:
| 运算符 | 含义 | 示例 |
|--------|------|------|
| `NOT` | 逻辑非 | `NOT is_active` |
| `-` | 负号 | `-amount` |
| `+` | 正号（通常省略） | `+value` |

**类定义**:
```cpp
class UnaryExpr : public Expr {
public:
    // ========== 核心数据成员 ==========
    std::string op_;                 // 运算符: "NOT", "-", "+"
    std::unique_ptr<Expr> operand_;  // 操作数

    // ========== 构造函数 ==========
    UnaryExpr() = default;
    UnaryExpr(std::string op, std::unique_ptr<Expr> operand)
        : op_(std::move(op)), operand_(std::move(operand)) {}

    // ========== 访问器 ==========
    const std::string& op() const { return op_; }
    const Expr* operand() const { return operand_.get(); }
    bool isNot() const { return op_ == "NOT"; }
    bool isNegation() const { return op_ == "-"; }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::EXPR_UNARY; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: NOT is_active

UnaryExpr { op: "NOT" }
└── operand_: ColumnRef { column: "is_active" }
```

---

#### LiteralExpr - 字面量表达式

**支持的类型**（复用 `TokenValue`）:
| 类型 | C++ 类型 | 示例 |
|------|----------|------|
| 整数 | `int64_t` | `42`, `-100` |
| 浮点 | `double` | `3.14`, `2.5e10` |
| 字符串 | `std::string` | `'hello'`, `"world"` |
| 布尔 | `bool` | `TRUE`, `FALSE` |
| 空值 | `std::monostate` | `NULL` |

**类定义**:
```cpp
class LiteralExpr : public Expr {
public:
    // ========== 核心数据成员 ==========
    TokenValue value_;  // 使用 variant<int64_t, double, string, bool, monostate>

    // ========== 构造函数 ==========
    LiteralExpr() : value_(std::monostate{}) {}  // 默认 NULL
    explicit LiteralExpr(TokenValue value) : value_(std::move(value)) {}

    // 便捷构造函数
    static LiteralExpr ofInt(int64_t v) { return LiteralExpr(v); }
    static LiteralExpr ofFloat(double v) { return LiteralExpr(v); }
    static LiteralExpr ofString(std::string v) { return LiteralExpr(std::move(v)); }
    static LiteralExpr ofBool(bool v) { return LiteralExpr(v); }
    static LiteralExpr ofNull() { return LiteralExpr(std::monostate{}); }

    // ========== 访问器 ==========
    const TokenValue& value() const { return value_; }

    // ========== 类型判断 ==========
    bool isNull() const { return std::holds_alternative<std::monostate>(value_); }
    bool isInt() const { return std::holds_alternative<int64_t>(value_); }
    bool isFloat() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }

    // ========== 值获取 ==========
    int64_t asInt() const { return std::get<int64_t>(value_); }
    double asFloat() const { return std::get<double>(value_); }
    const std::string& asString() const { return std::get<std::string>(value_); }
    bool asBool() const { return std::get<bool>(value_); }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::EXPR_LITERAL; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: 123

LiteralExpr { value: int64_t(123) }

输入: 'hello world'

LiteralExpr { value: std::string("hello world") }
```

---

#### ColumnRef - 列引用表达式

**SQL 语法**:
```sql
column_name           -- 简单列名
table_name.column_name -- 带表名前缀
```

**类定义**:
```cpp
class ColumnRef : public Expr {
public:
    // ========== 核心数据成员 ==========
    std::string table_;    // 表名（可空）
    std::string column_;   // 列名

    // ========== 构造函数 ==========
    ColumnRef() = default;
    explicit ColumnRef(std::string column)
        : column_(std::move(column)) {}
    ColumnRef(std::string table, std::string column)
        : table_(std::move(table)), column_(std::move(column)) {}

    // ========== 访问器 ==========
    const std::string& table() const { return table_; }
    const std::string& column() const { return column_; }
    bool hasTableQualifier() const { return !table_.empty(); }

    // ========== 便捷方法 ==========
    std::string fullName() const {
        return table_.empty() ? column_ : table_ + "." + column_;
    }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::EXPR_COLUMN_REF; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: users.id

ColumnRef
├── table_: "users"
└── column_: "id"

输入: name

ColumnRef
├── table_: ""  (空)
└── column_: "name"
```

---

#### IsNullExpr - NULL 判断表达式

**SQL 语法**:
```sql
expr IS NULL      -- 判断是否为 NULL
expr IS NOT NULL  -- 判断是否不为 NULL
```

**类定义**:
```cpp
class IsNullExpr : public Expr {
public:
    // ========== 核心数据成员 ==========
    std::unique_ptr<Expr> expr_;    // 被检查的表达式
    bool negated_ = false;          // IS NOT NULL 时为 true

    // ========== 构造函数 ==========
    IsNullExpr() = default;
    IsNullExpr(std::unique_ptr<Expr> expr, bool negated = false)
        : expr_(std::move(expr)), negated_(negated) {}

    // ========== 访问器 ==========
    const Expr* expr() const { return expr_.get(); }
    bool isNegated() const { return negated_; }
    bool isIsNotNull() const { return negated_; }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::EXPR_IS_NULL; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: email IS NULL

IsNullExpr { negated: false }
└── expr_: ColumnRef { column: "email" }

输入: phone IS NOT NULL

IsNullExpr { negated: true }
└── expr_: ColumnRef { column: "phone" }
```

### 4.7 辅助节点详细定义

#### ColumnDef - 列定义

**SQL 语法**:
```sql
column_name data_type [NOT NULL] [DEFAULT value]
```

**支持的数据类型**:
| 类型 | SQL 名称 | 说明 |
|------|----------|------|
| INT | `INT`, `INTEGER` | 32位整数 |
| BIGINT | `BIGINT` | 64位整数 |
| FLOAT | `FLOAT`, `REAL` | 单精度浮点 |
| DOUBLE | `DOUBLE`, `DOUBLE PRECISION` | 双精度浮点 |
| VARCHAR | `VARCHAR(n)` | 变长字符串，最大长度 n |
| TEXT | `TEXT` | 无限长文本 |
| BOOLEAN | `BOOLEAN`, `BOOL` | 布尔值 |

**类定义**:
```cpp
/// 数据类型信息
struct DataTypeInfo {
    DataType base_type_;                    // 基本类型
    std::optional<size_t> length_;          // VARCHAR(n) 的长度

    DataTypeInfo() = default;
    explicit DataTypeInfo(DataType type) : base_type_(type) {}
    DataTypeInfo(DataType type, size_t len) : base_type_(type), length_(len) {}

    bool hasLength() const { return length_.has_value(); }
    size_t length() const { return length_.value_or(0); }
};

class ColumnDef : public ASTNode {
public:
    // ========== 核心数据成员 ==========
    std::string name_;                          // 列名
    DataTypeInfo data_type_;                    // 数据类型信息
    bool nullable_ = true;                      // 是否允许 NULL
    std::optional<TokenValue> default_value_;   // DEFAULT 值

    // ========== 构造函数 ==========
    ColumnDef() = default;
    ColumnDef(std::string name, DataTypeInfo data_type)
        : name_(std::move(name)), data_type_(std::move(data_type)) {}

    // ========== 访问器 ==========
    const std::string& name() const { return name_; }
    const DataTypeInfo& dataType() const { return data_type_; }
    bool isNullable() const { return nullable_; }
    bool hasDefault() const { return default_value_.has_value(); }
    const TokenValue& defaultValue() const { return default_value_.value(); }

    // ========== 修改器 ==========
    void setName(std::string name) { name_ = std::move(name); }
    void setDataType(DataTypeInfo type) { data_type_ = std::move(type); }
    void setNullable(bool nullable) { nullable_ = nullable; }
    void setDefault(TokenValue val) { default_value_ = std::move(val); }

    // ========== 类型判断 ==========
    bool isInteger() const { return data_type_.base_type_ == DataType::INT ||
                                    data_type_.base_type_ == DataType::BIGINT; }
    bool isFloat() const { return data_type_.base_type_ == DataType::FLOAT ||
                                 data_type_.base_type_ == DataType::DOUBLE; }
    bool isString() const { return data_type_.base_type_ == DataType::VARCHAR ||
                                 data_type_.base_type_ == DataType::TEXT; }
    bool isBoolean() const { return data_type_.base_type_ == DataType::BOOLEAN; }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::COLUMN_DEF; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: name VARCHAR(100) NOT NULL DEFAULT 'unknown'

ColumnDef
├── name_: "name"
├── data_type_: DataTypeInfo { base_type: VARCHAR, length: 100 }
├── nullable_: false
└── default_value_: string("unknown")
```

---

#### TableRef - 表引用

**SQL 语法**:
```sql
table_name [AS] alias
```

**类定义**:
```cpp
class TableRef : public ASTNode {
public:
    // ========== 核心数据成员 ==========
    std::string name_;    // 表名
    std::string alias_;   // 别名（可空）

    // ========== 构造函数 ==========
    TableRef() = default;
    explicit TableRef(std::string name)
        : name_(std::move(name)) {}
    TableRef(std::string name, std::string alias)
        : name_(std::move(name)), alias_(std::move(alias)) {}

    // ========== 访问器 ==========
    const std::string& name() const { return name_; }
    const std::string& alias() const { return alias_; }
    bool hasAlias() const { return !alias_.empty(); }

    // ========== 便捷方法 ==========
    std::string effectiveName() const {
        return alias_.empty() ? name_ : alias_;
    }

    // ========== 虚函数实现 ==========
    NodeType type() const override { return NodeType::TABLE_REF; }

private:
    std::string toString() const override;
};
```

**AST 示例**:
```
输入: FROM users AS u

TableRef
├── name_: "users"
└── alias_: "u"

输入: FROM products

TableRef
├── name_: "products"
└── alias_: "" (空)
```

## 5. Parser 类设计

### 5.1 公共接口

```cpp
class Parser {
public:
    /// 从 Lexer 构造 Parser
    explicit Parser(Lexer& lexer);

    /// 解析单条 SQL 语句
    Result<std::unique_ptr<Stmt>> parse();

    /// 解析多条 SQL 语句（以分号分隔）
    Result<std::vector<std::unique_ptr<Stmt>>> parseAll();
};
```

### 5.2 语句解析方法

```cpp
private:
    /// 语句解析入口（根据首 Token 分发）
    Result<std::unique_ptr<Stmt>> parseStatement();

    /// DDL 语句
    Result<std::unique_ptr<CreateTableStmt>> parseCreateTable();
    Result<std::unique_ptr<DropTableStmt>> parseDropTable();

    /// DML 语句
    Result<std::unique_ptr<InsertStmt>> parseInsert();
    Result<std::unique_ptr<SelectStmt>> parseSelect();
    Result<std::unique_ptr<UpdateStmt>> parseUpdate();
    Result<std::unique_ptr<DeleteStmt>> parseDelete();
```

### 5.3 表达式解析方法

表达式解析采用分层递归下降，每层处理一个优先级。

```cpp
    /// 表达式解析入口
    Result<std::unique_ptr<Expr>> parseExpression();

    /// 分层解析（优先级从低到高）
    Result<std::unique_ptr<Expr>> parseOrExpr();          // OR
    Result<std::unique_ptr<Expr>> parseAndExpr();         // AND
    Result<std::unique_ptr<Expr>> parseNotExpr();         // NOT
    Result<std::unique_ptr<Expr>> parseComparisonExpr();  // =, <>, <, >, <=, >=
    Result<std::unique_ptr<Expr>> parseAdditiveExpr();    // +, -
    Result<std::unique_ptr<Expr>> parseConcatExpr();      // ||
    Result<std::unique_ptr<Expr>> parseMultiplicativeExpr(); // *, /, %
    Result<std::unique_ptr<Expr>> parseUnaryExpr();       // 一元 +, -
    Result<std::unique_ptr<Expr>> parsePrimaryExpr();     // 字面量、列引用、括号
```

### 5.4 表达式优先级

符合 SQL 标准的运算符优先级（从低到高）：

| 优先级 | 运算符 | 说明 |
|--------|--------|------|
| 1 | `OR` | 逻辑或 |
| 2 | `AND` | 逻辑与 |
| 3 | `NOT` | 逻辑非 |
| 4 | `=`, `<>`, `<`, `>`, `<=`, `>=` | 比较运算 |
| 5 | `+`, `-` | 加减运算 |
| 6 | `||` | 字符串拼接 |
| 7 | `*`, `/`, `%` | 乘除模运算 |
| 8 | `+`, `-` (一元) | 正负号 |
| 9 | `IS [NOT] NULL` | NULL 判断 |
| 10 | 字面量、列引用、括号 | 基本表达式 |

**示例解析**:
```sql
a + b * c           →  a + (b * c)
a OR b AND c        →  a OR (b AND c)
NOT a = b           →  NOT (a = b)
x IS NULL AND y > 0 →  (x IS NULL) AND (y > 0)
```

### 5.5 辅助解析方法

```cpp
    /// 解析列定义
    Result<std::unique_ptr<ColumnDef>> parseColumnDef();

    /// 解析表引用
    Result<std::unique_ptr<TableRef>> parseTableRef();

    /// 解析标识符
    Result<std::string> parseIdentifier();

    /// 解析字面量
    Result<TokenValue> parseLiteral();
```

### 5.6 Token 操作方法

```cpp
    /// 获取当前 Token（不消费）
    const Token& current() const;

    /// 消费并返回当前 Token
    Token consume();

    /// 期望特定类型的 Token
    Result<Token> expect(TokenType type, const char* message);

    /// 期望特定关键字
    Result<Token> expectKeyword(Keyword keyword, const char* message);

    /// 检查当前 Token 类型
    bool check(TokenType type) const;
    bool checkKeyword(Keyword keyword) const;

    /// 匹配并消费（如果匹配）
    bool match(TokenType type);
    bool matchKeyword(Keyword keyword);
```

### 5.7 错误处理

```cpp
    /// 生成语法错误
    template<typename T>
    Result<T> syntaxError(const std::string& message);

    /// 成员变量
    Lexer& lexer_;
    Token current_token_;
```

## 6. 错误处理策略

### 6.1 快速失败

- 遇到第一个语法错误立即停止
- 返回包含位置信息的详细错误消息
- 使用 `Result<T>` 模式，不抛异常

### 6.2 错误消息格式

```
Syntax error at line {line}, column {column}: {message}
```

**示例**:
```
Syntax error at line 3, column 15: expected ')' but found 'FROM'
Syntax error at line 1, column 20: unexpected token 'WHERE'
Syntax error at line 2, column 5: expected column name but found 'INT'
```

## 7. 测试策略

### 7.1 单元测试

测试文件: `tests/unit/parser/test_parser.cpp`

**测试用例类别**:

1. **语句解析测试**
   - CREATE TABLE 基本语法
   - CREATE TABLE 带约束
   - DROP TABLE / DROP TABLE IF EXISTS
   - INSERT 全列 / 指定列
   - SELECT * / SELECT columns / SELECT with WHERE
   - UPDATE with SET / UPDATE with WHERE
   - DELETE with WHERE

2. **表达式解析测试**
   - 各类运算符优先级
   - 嵌套表达式
   - 括号改变优先级
   - IS NULL / IS NOT NULL

3. **错误处理测试**
   - 语法错误位置
   - 期望 Token 不匹配
   - 无效的表达式

### 7.2 测试示例

```cpp
TEST_CASE("Parse CREATE TABLE") {
    std::string sql = "CREATE TABLE users (id INT, name TEXT)";
    Lexer lexer(sql);
    Parser parser(lexer);

    auto result = parser.parse();
    REQUIRE(result.isOk());

    auto stmt = result.unwrap();
    REQUIRE(stmt->type() == NodeType::STMT_CREATE_TABLE);

    auto* create = static_cast<CreateTableStmt*>(stmt.get());
    REQUIRE(create->table_name == "users");
    REQUIRE(create->columns.size() == 2);
}

TEST_CASE("Expression precedence: a + b * c") {
    std::string sql = "SELECT * FROM t WHERE a + b * c > 0";
    // 验证解析为: a + (b * c) > 0
    // 即: (a + (b * c)) > 0
}

TEST_CASE("Syntax error location") {
    std::string sql = "SELECT * FORM users";  // FORM 应为 FROM
    Lexer lexer(sql);
    Parser parser(lexer);

    auto result = parser.parse();
    REQUIRE(result.isErr());
    REQUIRE(result.error().message().find("line 1") != std::string::npos);
}
```

## 8. 实现注意事项

### 8.1 编码规范

遵循 [docs/CODING_STANDARDS.md](../CODING_STANDARDS.md):

- 类型命名: `PascalCase`
- 函数/变量命名: `snake_case`
- 成员变量命名: `snake_case_` (下划线后缀)
- 使用 `Result<T>` 错误处理，不抛异常
- 使用智能指针管理内存

### 8.2 与 Lexer 集成

- Parser 持有 Lexer 的引用
- 使用 Lexer 的 1-token 前瞻功能
- 复用 `Token`, `TokenValue`, `Location` 等类型

### 8.3 后续扩展

本设计预留的扩展点：

- **Visitor 模式**: `ast_visitor.h` 用于遍历 AST
- **更多语句类型**: JOIN, GROUP BY, ORDER BY 等
- **更多表达式**: LIKE, BETWEEN, IN, 子查询
- **错误恢复**: 可选的同步恢复机制

## 9. 参考资料

- [PostgreSQL Parser Stage](https://www.postgresql.org/docs/current/parser-stage.html)
- [SQLite Lemon Parser Generator](https://sqlite.org/lemon.html)
- SQL:2016 标准运算符优先级
