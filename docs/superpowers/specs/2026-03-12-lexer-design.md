# SeedDB SQL Lexer 设计文档

**日期**: 2026-03-12
**状态**: ✅ 实现完成
**阶段**: Phase 1.1 - Lexer

## 1. 概述

本文档描述 SeedDB SQL 词法分析器 (Lexer) 的设计方案。Lexer 负责将 SQL 输入字符串转换为 Token 流，供后续 Parser 解析。

### 1.1 设计目标

- **手写 C++ 实现**: 参考 SQLite/MySQL 方案，零依赖，高性能
- **扩展 SQL 子集支持**: 包含 JOIN, GROUP BY, ORDER BY, 子查询等
- **Fail-Fast 错误处理**: 遇错即停，清晰定位问题
- **完整位置追踪**: 支持友好的错误信息

### 1.2 业界参考

| 数据库 | Lexer 方案 | Lookahead |
|--------|-----------|-----------|
| PostgreSQL | Flex 生成器 | 1 token |
| SQLite | 手写 C | 1 token |
| MySQL | 手写 C++ | 1 token |

**结论**: SQL 语法设计为 LALR(1) 兼容，只需 1 token lookahead。

## 2. 架构设计

### 2.1 文件结构

```
src/parser/
├── token.h           # TokenType, TokenValue, Location, Token
├── lexer.h           # Lexer 类声明
├── lexer.cpp         # Lexer 实现
├── keywords.h        # 关键字查找表
└── CMakeLists.txt    # 模块构建配置

tests/unit/
└── test_lexer.cpp    # Lexer 单元测试
```

### 2.2 模块依赖

```
parser (新增)
    │
    ↓ 依赖
common (Phase 0)
    - Result<T> 错误处理
    - ErrorCode 枚举
    - string_utils
```

### 2.3 架构图

```
                    ┌─────────────────┐
   SQL String ───→  │     Lexer       │
                    │  ┌───────────┐  │
                    │  │ position_ │  │
                    │  └───────────┘  │
                    │  ┌───────────┐  │
                    │  │peek_buf_  │  │ ← 1 token lookahead
                    │  │(optional) │  │
                    │  └───────────┘  │
                    └─────────────────┘
                           │
            ┌──────────────┴──────────────┐
            ↓                              ↓
      next_token()                    peek_token()
   (消费并返回)                      (预览不消费)
```

## 3. Token 类型设计

### 3.1 TokenType 枚举

**注**: 标识符引用（如 `"table_name"`, `[table_name]`, \`table_name\`）暂不支持，
      Phase 1 仅支持未引用的简单标识符。后续阶段可扩展。

```cpp
enum class TokenType {
    // ===== DDL =====
    CREATE, DROP, ALTER, TABLE, INDEX, VIEW,

    // ===== DML =====
    SELECT, FROM, WHERE, INSERT, INTO, UPDATE, DELETE,
    VALUES, SET,

    // ===== JOIN =====
    JOIN, INNER, LEFT, RIGHT, OUTER, CROSS, ON, USING,

    // ===== GROUP BY / ORDER BY =====
    GROUP, BY, ORDER, ASC, DESC, HAVING,

    // ===== 子查询支持 =====
    EXISTS, IN, BETWEEN, LIKE, IS, AS,

    // ===== 数据类型 =====
    INTEGER, BIGINT, SMALLINT, FLOAT, DOUBLE, VARCHAR, CHAR, BOOLEAN,

    // ===== 约束 =====
    PRIMARY, KEY, FOREIGN, REFERENCES, UNIQUE, NOT, NULL_LIT, DEFAULT,

    // ===== 逻辑运算符 =====
    AND, OR, TRUE_LIT, FALSE_LIT,  // NOT 已在约束部分定义

    // ===== 字面量 =====
    INTEGER_LIT,    // value: int64_t
    FLOAT_LIT,      // value: double
    STRING_LIT,     // value: std::string
    IDENTIFIER,     // value: std::string

    // ===== 运算符 =====
    PLUS, MINUS, STAR, SLASH, PERCENT,    // + - * / %
    EQ, NE, LT, GT, LE, GE,               // = <> < > <= >=
    CONCAT,                                 // ||

    // ===== 分隔符 =====
    LPAREN, RPAREN,     // ( )
    LBRACKET, RBRACKET, // [ ]
    COMMA, SEMICOLON, DOT,

    // ===== 特殊 =====
    END_OF_INPUT,
    ERROR
};
```

### 3.2 TokenValue 类型

```cpp
using TokenValue = std::variant<
    std::monostate,    // 无值 (关键字、运算符等)
    int64_t,           // 整数字面量
    double,            // 浮点字面量
    std::string,       // 字符串字面量 / 标识符
    bool               // 布尔字面量
>;
```

### 3.3 Location 位置信息

```cpp
struct Location {
    size_t line;       // 行号 (1-based)
    size_t column;     // 列号 (1-based)
    size_t start;      // 起始位置 (0-based)
    size_t length;     // token 长度

    std::string to_string() const;
};
```

### 3.4 Token 结构

```cpp
struct Token {
    TokenType type;
    TokenValue value;
    Location loc;
};
```

## 4. Lexer 类设计

### 4.1 类接口

```cpp
namespace seeddb::parser {

/// SQL 词法分析器
///
/// NOT thread-safe. 每个实例应在单线程中使用。
/// 并发解析时请创建独立的 Lexer 实例。
class Lexer {
public:
    explicit Lexer(std::string_view input);

    // 核心接口
    Result<Token> next_token();    // 获取下一个 token (消费)
    Result<Token> peek_token();    // 预览下一个 token (不消费)

    // 状态查询
    bool has_more() const;
    Location current_location() const;

private:
    // 输入状态
    std::string_view input_;
    size_t position_;
    size_t line_;
    size_t column_;

    // 1-token lookahead 缓存
    std::optional<Token> peek_buffer_;

    // 内部扫描方法
    void skip_whitespace_and_comments();
    Result<Token> scan_identifier_or_keyword();
    Result<Token> scan_number();
    Result<Token> scan_string();
    Result<Token> scan_operator();
    Result<Token> scan_delimiter();

    // 辅助方法
    Token make_token(TokenType type, TokenValue value = std::monostate{});
    char peek_char(size_t offset = 0) const;
    char advance();
};

}  // namespace seeddb::parser
```

### 4.2 状态机流程

```
next_token()
    │
    ↓
┌─────────────────────┐
│ 检查 peek_buffer_   │──有──→ 返回缓存 token
└─────────────────────┘
    │ 无
    ↓
┌─────────────────────┐
│ 跳过空白和注释       │
└─────────────────────┘
    │
    ↓
┌─────────────────────┐
│ 检查是否 EOF        │──是──→ 返回 END_OF_INPUT
└─────────────────────┘
    │ 否
    ↓
┌─────────────────────┐
│ 根据首字符分派       │
│ [a-zA-Z_] → 标识符   │
│ [0-9]     → 数字     │
│ '         → 字符串   │
│ 其他      → 运算符   │
└─────────────────────┘
```

### 4.3 关键字识别

```cpp
// keywords.h
// 使用 inline 避免 ODR 违规 (C++17)
inline const std::unordered_map<std::string_view, TokenType> keywords = {
    {"SELECT", TokenType::SELECT},
    {"FROM", TokenType::FROM},
    {"WHERE", TokenType::WHERE},
    {"INSERT", TokenType::INSERT},
    {"UPDATE", TokenType::UPDATE},
    {"DELETE", TokenType::DELETE},
    // ... 完整列表见实现
};

// 关键字查找逻辑
Result<Token> Lexer::scan_identifier_or_keyword() {
    Location loc = current_location();
    size_t start = position_;

    // 扫描标识符字符 (使用 std::isalnum，需要转换为 unsigned char)
    while (position_ < input_.size() &&
           (std::isalnum(static_cast<unsigned char>(input_[position_])) ||
            input_[position_] == '_')) {
        advance();
    }

    std::string_view text = input_.substr(start, position_ - start);

    // 转大写后查关键字表
    std::string upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    auto it = keywords.find(upper);
    if (it != keywords.end()) {
        return make_token(it->second);
    }

    // 普通标识符
    return make_token(TokenType::IDENTIFIER, std::string(text));
}
```

## 5. 错误处理

### 5.1 Fail-Fast 策略

- 遇到词法错误立即返回 `Result<Token>::err()`
- 不尝试错误恢复
- 提供清晰的错误位置和描述

### 5.2 错误类型

| 场景 | 错误消息示例 |
|------|-------------|
| 非法字符 | `Unexpected character '@' at line 1, column 5` |
| 未闭合字符串 | `Unterminated string literal at line 1, column 1` |
| 无效数字 | `Invalid number format at line 1, column 1` |
| 无效转义 | `Invalid escape sequence '\x' at line 1, column 7` |

### 5.3 错误码扩展

在 `src/common/error.h` 中添加 Lexer 相关错误码：

```cpp
// 词法分析错误 (100-199)
UNEXPECTED_CHARACTER = 100,
UNTERMINATED_STRING = 101,
INVALID_NUMBER = 102,
INVALID_ESCAPE_SEQUENCE = 103,
```

### 5.4 错误返回方式

Lexer 使用项目已有的 `Result<Token>` 模式：

```cpp
Result<Token> Lexer::next_token() {
    // ...
    if (is_error_condition) {
        return Result<Token>::err(
            ErrorCode::UNEXPECTED_CHARACTER,
            fmt::format("Unexpected character '{}' at {}:{}",
                        c, line_, column_)
        );
    }
    // ...
}
```

### 5.5 错误信息格式

所有词法错误信息遵循统一格式：
```
Lexer error: {description} at line {L}, column {C}
```

## 6. 语句终结符

### 6.1 设计

- **终结符**: 分号 `;`
- **Token 类型**: `TokenType::SEMICOLON`
- **处理方式**: Lexer 返回 SEMICOLON token，由 Parser 负责语句分隔

### 6.2 多语句支持

```sql
SELECT 1;
INSERT INTO t VALUES (2);
```

Lexer 输出:
```
SELECT, INTEGER_LIT(1), SEMICOLON,
INSERT, INTO, IDENTIFIER("t"), VALUES, LPAREN, INTEGER_LIT(2), RPAREN, SEMICOLON,
END_OF_INPUT
```

## 7. 测试策略

### 7.1 测试分类

| 类别 | 测试内容 |
|------|---------|
| 关键字 | 所有 SQL 关键字识别 |
| 字面量 | 整数、浮点、字符串、布尔 |
| 标识符 | 表名、列名、带引号标识符 |
| 运算符 | 算术、比较、逻辑运算符 |
| 分隔符 | 括号、逗号、分号、点 |
| 位置追踪 | 行号、列号正确性 |
| 错误处理 | 各种错误场景 |
| Peek | lookahead 不消费 |

### 7.2 测试用例示例

```cpp
TEST_CASE("Lexer: Keywords", "[lexer]") {
    Lexer lexer("SELECT FROM WHERE");
    REQUIRE(lexer.next_token().unwrap().type == TokenType::SELECT);
    REQUIRE(lexer.next_token().unwrap().type == TokenType::FROM);
    REQUIRE(lexer.next_token().unwrap().type == TokenType::WHERE);
}

TEST_CASE("Lexer: Integer literal", "[lexer]") {
    Lexer lexer("12345");
    auto tok = lexer.next_token().unwrap();
    REQUIRE(tok.type == TokenType::INTEGER_LIT);
    REQUIRE(std::get<int64_t>(tok.value) == 12345);
}

TEST_CASE("Lexer: String literal", "[lexer]") {
    Lexer lexer("'hello world'");
    auto tok = lexer.next_token().unwrap();
    REQUIRE(tok.type == TokenType::STRING_LIT);
    REQUIRE(std::get<std::string>(tok.value) == "hello world");
}

TEST_CASE("Lexer: Location tracking", "[lexer]") {
    Lexer lexer("SELECT\n  FROM");
    auto tok1 = lexer.next_token().unwrap();
    REQUIRE(tok1.loc.line == 1);
    REQUIRE(tok1.loc.column == 1);

    auto tok2 = lexer.next_token().unwrap();
    REQUIRE(tok2.loc.line == 2);
    REQUIRE(tok2.loc.column == 3);
}

TEST_CASE("Lexer: Error - unexpected character", "[lexer]") {
    Lexer lexer("SELECT @ FROM");
    REQUIRE(lexer.next_token().unwrap().type == TokenType::SELECT);
    auto result = lexer.next_token();
    REQUIRE(result.is_err());
}

TEST_CASE("Lexer: Peek token", "[lexer]") {
    Lexer lexer("SELECT FROM");
    auto peeked = lexer.peek_token().unwrap();
    REQUIRE(peeked.type == TokenType::SELECT);

    // peek 不消费
    auto tok = lexer.next_token().unwrap();
    REQUIRE(tok.type == TokenType::SELECT);
}
```

## 8. 实现步骤

| 步骤 | 任务 | 产出文件 |
|------|------|---------|
| 1 | 创建 parser 模块目录和 CMakeLists | `src/parser/CMakeLists.txt` |
| 2 | 定义 Token 类型 | `src/parser/token.h` |
| 3 | 定义关键字查找表 | `src/parser/keywords.h` |
| 4 | 实现 Lexer 类框架 | `src/parser/lexer.h`, `lexer.cpp` |
| 5 | 实现各扫描方法 | 更新 `lexer.cpp` |
| 6 | 编写单元测试 | `tests/unit/test_lexer.cpp` |
| 7 | 集成到主构建 | 更新 `CMakeLists.txt` |

## 9. 验收标准

- [x] 所有 TokenType 正确定义
- [x] 关键字识别准确（大小写不敏感）
- [x] 字面量正确解析并存储值
- [x] 位置信息 (line, column) 正确
- [x] peek_token() 不消费 token
- [x] 错误场景返回 Result::err()
- [x] 单元测试覆盖率 > 90%
- [x] 构建通过，无警告

## 10. 参考资料

- [PostgreSQL scan.l](https://github.com/postgres/postgres/blob/master/src/backend/parser/scan.l)
- [SQLite tokenize.c](https://sqlite.org/arch.html)
- [MySQL sql_lex.cc](https://dev.mysql.com/doc/dev/mysql-server/8.4.8/sql__lex_8cc.html)
- [Error Recovery Strategies](https://www.geeksforgeeks.org/compiler-design/error-recovery-strategies-in-compiler-design/)
