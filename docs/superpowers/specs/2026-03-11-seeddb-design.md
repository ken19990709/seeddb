# SeedDB - 轻量级数据库内核项目设计

## 1. Context（背景与目标）

### 1.1 项目动机
作为一个专注于 SQL 引擎的数据库内核开发工程师，希望通过从零构建一个完整的数据库系统，深入理解存储引擎、事务系统等非 SQL 模块的核心原理。

### 1.2 项目目标
- **主要目标**：补全对数据库内核全栈的理解（存储、事务、并发）
- **次要目标**：构建一个可长期演进的轻量级数据库，作为技术能力的展示

### 1.3 约束条件
- 时间投入：长期演进项目（6个月以上）
- 开发平台：Linux
- 代码风格：遵循 PostgreSQL 风格

---

## 2. 项目配置

| 配置项 | 决策 |
|--------|------|
| **项目名称** | SeedDB |
| **编程语言** | C++ (后续可迁移 Rust) |
| **构建系统** | CMake |
| **测试框架** | Catch2 |
| **SQL 兼容** | PostgreSQL 语法子集 |
| **存储引擎** | 内存存储 → B+ 树 |
| **事务支持** | 渐进式 ACID |
| **并发模型** | 多线程 + 连接池 |
| **参考项目** | PostgreSQL, DuckDB, SQLite |

---

## 3. 架构设计

### 3.1 整体架构（参考 PostgreSQL + DuckDB 设计）

#### 3.1.1 完整架构图

```
┌──────────────────────────────────────────────────────────────────────────┐
│                            Client Layer                                   │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐  │
│  │   libseeddb     │  │   psql 兼容     │  │   其他客户端 (JDBC等)    │  │
│  │  (C客户端库)    │  │   客户端工具    │  │   (通过 PG 协议)         │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
                                    │ TCP/IP / Unix Socket
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         Server Process                                    │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    Connection Manager                                │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │ │
│  │  │  Listener    │  │  Session     │  │  Thread Pool             │  │ │
│  │  │  (监听连接)  │  │  (会话管理)  │  │  (工作线程池)            │  │ │
│  │  └──────────────┘  └──────────────┘  └──────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                      │
│                                    ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    Protocol Layer                                    │ │
│  │  ┌──────────────────────────────────────────────────────────────┐  │ │
│  │  │  PostgreSQL Wire Protocol (v3.0)                             │  │ │
│  │  │  - Startup Message, Query, Parse, Bind, Execute, Sync        │  │ │
│  │  └──────────────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                      │
│                                    ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    SQL Engine (Plugin-based)                         │ │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────────────────────┐    │ │
│  │  │  Parser    │→ │ Optimizer  │→ │  Executor                  │    │ │
│  │  │ (可插拔)   │  │ (可插拔)   │  │  (向量化算子, 可插拔)       │    │ │
│  │  └────────────┘  └────────────┘  └────────────────────────────┘    │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                      │
│                                    ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    Storage Engine (Plugin-based)                     │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐    │ │
│  │  │ Buffer Pool  │  │ Index Manager│  │ Table Manager          │    │ │
│  │  │ (可替换策略) │  │ (B+Tree/Hash)│  │ (Row/Column 可选)      │    │ │
│  │  └──────────────┘  └──────────────┘  └────────────────────────┘    │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                      │
│                                    ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    Transaction Manager                               │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐    │ │
│  │  │ Lock Manager │  │ MVCC Manager │  │ WAL & Recovery         │    │ │
│  │  └──────────────┘  └──────────────┘  └────────────────────────┘    │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                      │
│                                    ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    Disk Manager                                      │ │
│  │  ┌──────────────────────────────────────────────────────────────┐  │ │
│  │  │  File I/O Abstraction (支持不同存储后端)                       │  │ │
│  │  └──────────────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         Extension System (DuckDB-style)                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐ │
│  │ Function    │  │ Type        │  │ Storage     │  │ Parser          │ │
│  │ Extensions  │  │ Extensions  │  │ Extensions  │  │ Extensions      │ │
│  │ (UDF/UDAF)  │  │ (自定义类型)│  │ (新存储引擎)│  │ (新语法支持)    │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
```

#### 3.1.2 客户端架构（参考 PostgreSQL libpq）

```
┌─────────────────────────────────────────────────────────────┐
│                    Client Library (libseeddb)                │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                   Connection API                         ││
│  │  seeddb_connect(), seeddb_disconnect(), seeddb_reset()  ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │                   Query API                              ││
│  │  seeddb_exec(), seeddb_prepare(), seeddb_execute()      ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │                   Result API                             ││
│  │  seeddb_result_fetch(), seeddb_get_value(), ...         ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │                   Protocol Implementation                ││
│  │  PostgreSQL Wire Protocol v3.0 (消息封装/解析)           ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

#### 3.1.3 扩展系统设计（参考 DuckDB）

| 扩展点 | 说明 | 示例 |
|--------|------|------|
| **Function Extension** | UDF/UDAF | 自定义标量函数、聚合函数 |
| **Type Extension** | 自定义数据类型 | JSON, UUID, Array |
| **Storage Extension** | 存储引擎插件 | LSM-Tree, Columnar Storage |
| **Parser Extension** | 语法扩展 | 新 SQL 语法、自定义命令 |
| **Index Extension** | 索引类型 | GIN, GiST, Vector Index |

#### 3.1.4 架构设计原则（参考 PG/DuckDB）

| 原则 | PostgreSQL 实践 | DuckDB 实践 | SeedDB 采用 |
|------|-----------------|-------------|-------------|
| **模块化** | 独立进程模块 | 插件化架构 | 插件化 + 接口抽象 |
| **扩展性** | Extension 机制 | Community Extensions | 扩展点预留 |
| **协议兼容** | libpq 客户端库 | 嵌入式 API | libseeddb + PG 协议 |
| **存储抽象** | Table Access Method | 可插拔存储 | Storage Engine Interface |

### 3.2 核心模块划分

```
seeddb/
├── CMakeLists.txt
├── src/
│   ├── common/              # 公共工具
│   │   ├── types.h          # 基础类型定义
│   │   ├── error.h          # 错误处理
│   │   ├── logger.h         # 日志系统
│   │   └── config.h         # 配置管理
│   │
│   ├── parser/              # SQL 解析器
│   │   ├── lexer.h/cpp      # 词法分析
│   │   ├── parser.h/cpp     # 语法分析
│   │   └── ast.h            # 抽象语法树
│   │
│   ├── optimizer/           # 查询优化器
│   │   ├── planner.h/cpp    # 逻辑计划生成
│   │   ├── rules.h/cpp      # 优化规则
│   │   └── plan.h           # 执行计划表示
│   │
│   ├── executor/            # 执行引擎
│   │   ├── executor.h/cpp   # 执行器框架
│   │   ├── operators.h/cpp  # 算子实现
│   │   └── expression.h/cpp # 表达式计算
│   │
│   ├── storage/             # 存储引擎
│   │   ├── buffer/          # Buffer Pool
│   │   ├── index/           # B+ 树索引
│   │   ├── table/           # 表存储
│   │   └── disk/            # 磁盘管理
│   │
│   ├── transaction/         # 事务管理
│   │   ├── lock_manager.h   # 锁管理器
│   │   ├── mvcc.h           # MVCC 实现
│   │   ├── wal.h            # WAL 日志
│   │   └── recovery.h       # 崩溃恢复
│   │
│   ├── protocol/            # 通信协议
│   │   └── postgres.h/cpp   # PostgreSQL 协议
│   │
│   └── server/              # 服务器
│       ├── connection.h/cpp # 连接管理
│       └── main.cpp         # 入口
│
├── tests/                   # 测试
│   ├── unit/                # 单元测试
│   └── integration/         # 集成测试
│
├── docs/                    # 文档
└── third_party/             # 第三方库
```

---

## 4. 开发阶段规划（细粒度、可验证）

> **原则**：每个任务独立、可验证、不超过 2-3 天工作量

---

### Phase 0: 项目搭建（1 周）✅ 已完成

> **完成日期**: 2026-03-11
> **完成内容**: 基础设施 + C++ 编码规范

#### Week 1: 基础设施
| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| P0-1 创建 Git 仓库 + 目录结构 | 0.5 天 | `tree seeddb/` 显示正确结构 | ✅ |
| P0-2 CMake 基础配置 | 0.5 天 | `cmake .. && make` 成功编译空项目 | ✅ |
| P0-3 Catch2 集成 + 示例测试 | 0.5 天 | `ctest` 运行并通过示例测试 | ✅ |
| P0-4 基础类型定义 (types.h) | 0.5 天 | 编译通过，类型大小正确 | ✅ |
| P0-5 错误处理框架 (error.h) | 0.5 天 | 错误码定义 + Result<T> 测试通过 | ✅ |
| P0-6 日志系统 (logger.h) | 0.5 天 | 日志输出到文件正确 | ✅ |
| P0-7 配置管理 (config.h) | 0.5 天 | 读取配置文件成功 | ✅ |
| P0-8 C++ 编码规范 | 0.5 天 | 文档化命名/内存/错误/字符串规范 | ✅ |

**里程碑验证**：`ctest` 全部通过，空项目可编译运行 ✅

**附加产出**：
- [CLAUDE.md](../../CLAUDE.md) - 项目记忆与快速参考
- [docs/CODING_STANDARDS.md](../../CODING_STANDARDS.md) - 完整 C++ 编码规范

---

### Phase 1: 内存存储 + 最小 SQL（6-8 周）

#### 1.1 词法分析器（1 周）✅ 已完成

> **完成日期**: 2026-03-12
> **详细设计**: [2026-03-12-lexer-design.md](2026-03-12-lexer-design.md)

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| L1-1 Token 类型定义 | 0.5 天 | 所有 SQL 关键字 Token 定义完成 | ✅ |
| L1-2 Lexer 框架 + 字符读取 | 0.5 天 | 读取字符串并返回字符流 | ✅ |
| L1-3 跳过空白和注释 | 0.5 天 | 测试用例：跳过空格/tab/注释 | ✅ |
| L1-4 识别关键字和标识符 | 1 天 | `SELECT`, `FROM`, `table_name` 正确识别 | ✅ |
| L1-5 识别数字和字符串字面量 | 1 天 | `123`, `3.14`, `'hello'` 正确识别 | ✅ |
| L1-6 识别运算符和符号 | 0.5 天 | `=`, `<`, `>`, `(`, `)`, `,` 正确识别 | ✅ |
| L1-7 Lexer 单元测试 | 0.5 天 | 覆盖所有 Token 类型的测试用例 | ✅ |

**里程碑验证**：Lexer 能正确解析 `SELECT * FROM users WHERE id = 1` ✅

#### 1.2 语法分析器（2 周）✅ 已完成

> **完成日期**: 2026-03-17

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| P1-1 AST 节点基类定义 | 0.5 天 | `ASTNode` 基类 + `toString()` 方法 | ✅ |
| P1-2 CREATE TABLE AST | 1 天 | 解析 `CREATE TABLE t (a INT, b VARCHAR(10))` | ✅ |
| P1-3 DROP TABLE AST | 0.5 天 | 解析 `DROP TABLE t` | ✅ |
| P1-4 INSERT AST | 1 天 | 解析 `INSERT INTO t VALUES (1, 'a')` | ✅ |
| P1-5 SELECT 基础 AST | 1 天 | 解析 `SELECT a, b FROM t` | ✅ |
| P1-6 SELECT + WHERE AST | 1 天 | 解析 `SELECT * FROM t WHERE a > 1` | ✅ |
| P1-7 UPDATE AST | 1 天 | 解析 `UPDATE t SET a = 1 WHERE b = 2` | ✅ |
| P1-8 DELETE AST | 0.5 天 | 解析 `DELETE FROM t WHERE a = 1` | ✅ |
| P1-9 表达式 AST | 1 天 | 解析 `a + b * 2`, `a IS NULL`, `a AND b` | ✅ |
| P1-10 Parser 单元测试 | 1 天 | 所有 SQL 语句解析测试通过 | ✅ |

**里程碑验证**：Parser 能正确解析所有基础 SQL 并生成 AST ✅

#### 1.3 内存存储层 + 1.4 执行引擎（合并实现）

> **设计日期**: 2026-03-17
> **设计原则**: Iterator 执行模型 + 向量化预留 + DuckDB 风格类型系统

##### 1.3.1 架构概览

```
src/
├── common/
│   ├── logical_type.h      # LogicalType, LogicalTypeId
│   └── value.h             # Value 类
├── storage/
│   ├── row.h               # Row 类
│   ├── schema.h            # ColumnSchema, Schema
│   ├── table.h             # Table 类
│   └── catalog.h           # Catalog 类
└── executor/
    └── executor.h          # ExecutionResult, Executor

tests/unit/
├── common/
│   ├── test_types.cpp      # 扩展：加入 LogicalType 测试
│   └── test_value.cpp      # 新增：Value + Row 合并测试
└── storage/
    ├── test_storage.cpp    # 新增：Schema + Table + Catalog 合并测试
    └── test_executor.cpp   # 新增：Executor 测试
```

**依赖关系**:
```
LogicalType  ←  Value  ←  Row
                      ↓
                   Schema  ←  Table  ←  Catalog
                                          ↓
AST (parser)  ←─────────────────────── Executor
```

##### 1.3.2 类型系统 (LogicalType)

```cpp
// src/common/logical_type.h

enum class LogicalTypeId {
    SQL_NULL,
    INTEGER,    // int32_t
    BIGINT,     // int64_t
    FLOAT,      // float
    DOUBLE,     // double
    VARCHAR,    // std::string
    BOOLEAN,    // bool
    // Future: DATE, TIMESTAMP, DECIMAL
};

class LogicalType {
public:
    explicit LogicalType(LogicalTypeId id = LogicalTypeId::SQL_NULL);
    LogicalTypeId id() const;

    bool isNumeric() const;
    bool isInteger() const;
    bool isFloating() const;
    bool isString() const;
    size_t fixedSize() const;  // 固定类型返回字节大小，变长返回0

private:
    LogicalTypeId id_;
};
```

**设计要点**:
- 区分数据库类型（INTEGER, DATE）与 C++ 存储类型（int32_t）
- 为未来扩展预留空间（DATE, TIMESTAMP, DECIMAL）

##### 1.3.3 Value 类

```cpp
// src/common/value.h

class Value {
public:
    // 构造
    Value();  // 默认 NULL
    static Value null();
    static Value integer(int32_t v);
    static Value bigint(int64_t v);
    static Value Float(float v);      // 避免与宏冲突
    static Value Double(double v);
    static Value varchar(std::string v);
    static Value boolean(bool v);

    // 类型查询
    const LogicalType& type() const;
    LogicalTypeId typeId() const;
    bool isNull() const;

    // 值访问（非 NULL 时调用）
    int32_t asInt32() const;
    int64_t asInt64() const;
    float asFloat() const;
    double asDouble() const;
    const std::string& asString() const;
    bool asBool() const;

    // 比较
    bool equals(const Value& other) const;
    bool lessThan(const Value& other) const;

    // 字符串表示（调试用）
    std::string toString() const;

private:
    LogicalType type_;
    bool is_null_ = true;
    union Storage {
        int32_t int32_val;
        int64_t int64_val;
        float float_val;
        double double_val;
        bool bool_val;
    } storage_;
    std::string str_val_;  // 字符串单独处理，不放在 union
};
```

**设计要点**:
- 字符串使用单独成员 `str_val_`，因为 `std::string` 有构造/析构函数
- 类型比较需要先检查 `LogicalTypeId` 是否兼容
- 预留 `equals`/`lessThan` 方法，为后续 WHERE 子句求值做准备

##### 1.3.4 Row 类

```cpp
// src/storage/row.h

class Row {
public:
    Row() = default;
    explicit Row(std::vector<Value> values);

    // 访问
    size_t size() const;
    const Value& get(size_t idx) const;
    Value& get(size_t idx);

    // 修改
    void append(Value value);
    void set(size_t idx, Value value);

    // 工具
    bool empty() const;
    void clear();
    std::string toString() const;

private:
    std::vector<Value> values_;
};
```

**设计要点**:
- 简单的类型擦除容器，每个 `Value` 携带自己的 `LogicalType`
- 与 Schema 解耦 —— Row 不知道自己属于哪个表，Schema 负责验证
- 为向量化执行预留：未来可以添加 `RowBatch` 类包装 `std::vector<Row>`

##### 1.3.5 Schema 类

```cpp
// src/storage/schema.h

class ColumnSchema {
public:
    ColumnSchema(std::string name, LogicalType type, bool nullable = true);

    const std::string& name() const;
    const LogicalType& type() const;
    bool isNullable() const;

private:
    std::string name_;
    LogicalType type_;
    bool nullable_;
};

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns);

    // 列信息
    size_t columnCount() const;
    const ColumnSchema& column(size_t idx) const;
    const ColumnSchema& column(const std::string& name) const;

    // 查找
    bool hasColumn(const std::string& name) const;
    size_t columnIndex(const std::string& name) const;  // 找不到返回 -1 或抛异常

    // 验证
    bool validateRow(const Row& row) const;  // 检查列数、类型兼容性

    // 工具
    std::string toString() const;

private:
    std::vector<ColumnSchema> columns_;
    std::unordered_map<std::string, size_t> name_to_index_;  // 列名 -> 索引缓存
};
```

**设计要点**:
- `ColumnSchema` 封装单列元信息：名称、类型、是否可空
- `Schema` 维护列名到索引的哈希表，加速按名查找
- `validateRow` 用于 INSERT 时验证数据合法性

##### 1.3.6 Table 类

```cpp
// src/storage/table.h

class Table {
public:
    Table(std::string name, Schema schema);

    // 元信息
    const std::string& name() const;
    const Schema& schema() const;
    size_t rowCount() const;

    // 数据操作
    void insert(Row row);
    bool remove(size_t idx);  // 返回是否成功
    void update(size_t idx, Row row);

    // 访问
    const Row& get(size_t idx) const;

    // 迭代器（支持 range-based for）
    auto begin() const { return rows_.begin(); }
    auto end() const { return rows_.end(); }

    // 清空
    void clear();

private:
    std::string name_;
    Schema schema_;
    std::vector<Row> rows_;
};
```

**设计要点**:
- 简单的内存存储，使用 `std::vector<Row>`
- 当前阶段用物理删除（简单）
- 支持迭代器，方便执行引擎遍历
- 后续持久化阶段会替换底层存储结构

##### 1.3.7 Catalog 类

```cpp
// src/storage/catalog.h

class Catalog {
public:
    Catalog() = default;

    // 表管理
    bool createTable(std::string name, Schema schema);
    bool dropTable(const std::string& name);
    bool hasTable(const std::string& name) const;

    // 表访问
    Table* getTable(const std::string& name);
    const Table* getTable(const std::string& name) const;

    // 迭代所有表
    auto begin() const { return tables_.begin(); }
    auto end() const { return tables_.end(); }
    size_t tableCount() const;

private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
};
```

**设计要点**:
- 管理所有表的元数据容器
- 使用 `unique_ptr<Table>` 保证指针稳定性（`unordered_map` 扩容时会移动元素）
- 表名作为 key，支持快速查找
- 后续可扩展：索引管理、视图、系统表等

##### 1.3.8 Executor 执行引擎

```cpp
// src/executor/executor.h

/// 执行结果 - 单行返回
class ExecutionResult {
public:
    enum class Status { OK, ERROR, EMPTY };

    static ExecutionResult ok(Row row);
    static ExecutionResult error(std::string message);
    static ExecutionResult empty();

    Status status() const;
    const Row& row() const;
    const std::string& errorMessage() const;
    bool hasRow() const;

private:
    Status status_;
    std::optional<Row> row_;
    std::string error_message_;
};

/// 执行引擎
class Executor {
public:
    explicit Executor(Catalog& catalog);

    /// 执行 DDL (CREATE/DROP TABLE)
    ExecutionResult execute(const CreateTableStmt& stmt);
    ExecutionResult execute(const DropTableStmt& stmt);

    /// 执行 DML (INSERT/SELECT/UPDATE/DELETE)
    ExecutionResult execute(const InsertStmt& stmt);
    ExecutionResult execute(const SelectStmt& stmt);  // 返回第一行，后续用迭代
    ExecutionResult execute(const UpdateStmt& stmt);
    ExecutionResult execute(const DeleteStmt& stmt);

    /// 迭代查询结果（SELECT 后调用）
    bool hasNext();
    ExecutionResult next();

private:
    Catalog& catalog_;
    // 当前查询状态（SELECT 用）
    const Table* current_query_table_ = nullptr;
    size_t current_row_index_ = 0;
};
```

**设计要点**:
- 接收 AST 节点，执行对应操作
- SELECT 使用迭代模型：首次调用返回第一行，后续通过 `hasNext()`/`next()` 遍历
- `ExecutionResult` 封装执行状态，便于错误处理
- 与 Parser 解耦 —— Executor 只依赖 AST 类型定义
- 预留向量化接口：当前迭代器模型可无缝扩展为 batch 版本

##### 1.3.9 任务分解

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| S1-1 LogicalType 实现 | 0.5 天 | 类型枚举、辅助方法、测试通过 |
| S1-2 Value 类实现 | 1 天 | 所有静态工厂方法、类型检查、比较方法 |
| S1-3 Row 类实现 | 0.5 天 | 增删改查、迭代器支持 |
| S1-4 Schema 类实现 | 1 天 | 列管理、按名查找、行验证 |
| S1-5 Table 类实现 | 1 天 | 内存存储、CRUD 操作 |
| S1-6 Catalog 类实现 | 1 天 | 表管理、按名查找 |
| S1-7 Executor 框架 | 0.5 天 | ExecutionResult、Executor 骨架 |
| S1-8 DDL 执行 (CREATE/DROP) | 1 天 | 通过 Catalog 创建/删除表 |
| S1-9 INSERT 执行 | 1 天 | AST → Value 转换、插入 Table |
| S1-10 SELECT 执行 | 1.5 天 | 全表扫描、迭代返回 |
| S1-11 UPDATE/DELETE 执行 | 1 天 | 条件匹配、修改/删除行 |
| S1-12 表达式求值 | 1.5 天 | BinaryExpr/UnaryExpr/IsNullExpr 求值 |
| S1-13 集成测试 | 1 天 | 端到端 SQL 执行流程 |

**里程碑验证**：端到端执行 `INSERT INTO t VALUES (1, 'a'); SELECT * FROM t;` 成功

##### 1.3.10 测试计划（精简版）

```
tests/unit/
├── common/
│   ├── test_types.cpp        # 扩展：加入 LogicalType 测试
│   └── test_value.cpp        # 新增：Value + Row 合并测试
└── storage/
    ├── test_storage.cpp      # 新增：Schema + Table + Catalog 合并测试
    └── test_executor.cpp     # 新增：Executor 测试（依赖完整流程）
```

**精简原则**:
- 7 个测试文件 → 4 个测试文件
- `LogicalType` 合并到现有 `test_types.cpp`
- `Value` + `Row` 合并（Row 测试依赖 Value）
- `Schema` + `Table` + `Catalog` 合并为 `test_storage.cpp`（三个类紧密相关）
- `Executor` 保持独立（是集成测试，需要完整环境）

#### 1.5 命令行工具（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| C1-1 简单 REPL | 1 天 | 读取用户输入，输出结果 |
| C1-2 SQL 执行集成 | 1 天 | REPL 中执行 SQL 并显示结果 |
| C1-3 结果格式化输出 | 1 天 | 表格格式输出，对齐正确 |
| C1-4 错误提示 | 1 天 | SQL 错误时显示友好提示 |

**里程碑验证**：命令行工具能交互式执行 SQL

---

### Phase 2: 多线程 + PostgreSQL 协议（4 周）

#### 2.1 线程框架（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| T2-1 线程池实现 | 1 天 | 固定大小线程池，任务提交和执行 |
| T2-2 Mutex 封装 | 0.5 天 | `Mutex`, `LockGuard` RAII 封装 |
| T2-3 ConditionVariable 封装 | 0.5 天 | 条件变量封装 |
| T2-4 线程安全队列 | 1 天 | 生产者-消费者模式测试通过 |
| T2-5 并发测试 | 1 天 | 多线程插入/查询测试通过 |

#### 2.2 连接管理（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| T2-6 Socket 监听 | 1 天 | 监听端口，接受连接 |
| T2-7 Connection 类 | 1 天 | 封装 socket，读写操作 |
| T2-8 Session 类 | 1 天 | 管理会话状态（当前数据库、事务等） |
| T2-9 Connection-Session 映射 | 1 天 | 每个连接对应一个 Session |

#### 2.3 PostgreSQL 协议（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| P2-1 消息读写框架 | 1 天 | 消息格式封装（类型+长度+内容） |
| P2-2 Startup 消息处理 | 1 天 | 解析启动消息，返回认证成功 |
| P2-3 Simple Query 协议 | 2 天 | 处理 Q 消息，返回结果 |
| P2-4 RowDescription 消息 | 1 天 | 描述返回列的类型 |
| P2-5 DataRow 消息 | 1 天 | 返回数据行 |
| P2-6 CommandComplete 消息 | 0.5 天 | 返回命令完成状态 |
| P2-7 ErrorResponse 消息 | 0.5 天 | 返回错误信息 |
| P2-8 协议集成测试 | 1 天 | psql 连接成功，执行查询 |

**里程碑验证**：`psql -h localhost -p 5432` 连接成功并执行 SQL

---

### Phase 3: B+ 树存储引擎（6 周）

#### 3.1 页面管理（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| D3-1 Page ID 设计 | 0.5 天 | 页面唯一标识 |
| D3-2 Page Header | 0.5 天 | 页头：页号、空闲空间、槽位数 |
| D3-3 Slotted Page 格式 | 2 天 | 变长数据存储，槽位数组 |
| D3-4 页面读写 | 1 天 | 页面序列化/反序列化 |
| D3-5 磁盘文件管理 | 1 天 | 文件打开/关闭，页面读写 |

#### 3.2 Buffer Pool（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| B3-1 Frame 定义 | 0.5 天 | Buffer Pool 中的页帧 |
| B3-2 Buffer Pool 框架 | 1 天 | 固定大小的页面缓存池 |
| B3-3 页面获取 (FetchPage) | 1 天 | 从磁盘读取页面到内存 |
| B3-4 页面刷盘 (FlushPage) | 1 天 | 脏页写回磁盘 |
| B3-5 LRU 替换策略 | 2 天 | 淘汰最久未使用的页面 |
| B3-6 Pin/Unpin 机制 | 1 天 | 引用计数，防止淘汰 |
| B3-7 页面锁 (PageLatch) | 1 天 | 读写锁，并发访问控制 |
| B3-8 Buffer Pool 并发测试 | 1 天 | 多线程访问测试通过 |

#### 3.3 B+ 树（3 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| I3-1 B+ 树节点定义 | 1 天 | InternalNode, LeafNode |
| I3-2 节点搜索 | 1 天 | 在节点内查找键 |
| I3-3 叶子节点插入 | 2 天 | 插入键值对，无分裂 |
| I3-4 节点分裂 | 2 天 | 节点满时分裂 |
| I3-5 内部节点插入 | 2 天 | 插入子节点指针 |
| I3-6 B+ 树查找 | 1 天 | 从根到叶子查找 |
| I3-7 B+ 树插入 | 2 天 | 完整插入流程（含分裂） |
| I3-8 B+ 树删除 | 2 天 | 删除 + 合并（简化版） |
| I3-9 范围扫描 | 1 天 | 叶子节点链表遍历 |
| I3-10 B+ 树并发控制 | 2 天 | crabbing 协议 |
| I3-11 B+ 树集成测试 | 1 天 | 大量数据插入/查询测试 |

**里程碑验证**：数据持久化到磁盘，重启后数据不丢失

---

### Phase 4: 事务系统（6 周）

#### 4.1 事务框架（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| X4-1 事务 ID 生成器 | 0.5 天 | 全局递增事务 ID |
| X4-2 Transaction 对象 | 1 天 | 事务状态、ID、开始时间戳 |
| X4-3 TransactionManager | 1 天 | BEGIN/COMMIT/ROLLBACK 管理 |
| X4-4 事务状态机 | 1 天 | 状态转换正确 |
| X4-5 事务隔离级别 | 0.5 天 | 支持 READ COMMITTED |

#### 4.2 MVCC（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| M4-1 版本链设计 | 1 天 | 每行指向旧版本的指针 |
| M4-2 xmin/xmax 字段 | 1 天 | 创建/删除事务 ID |
| M4-3 ReadView | 2 天 | 快照，判断可见性 |
| M4-4 可见性判断算法 | 2 天 | 正确判断行对事务是否可见 |
| M4-5 版本垃圾回收 (VACUUM) | 1 天 | 清理旧版本（简化版） |

#### 4.3 锁管理器（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| L4-1 Lock Request | 0.5 天 | 锁请求对象 |
| L4-2 Lock Manager 框架 | 1 天 | 锁表管理 |
| L4-3 表锁实现 | 2 天 | S锁/X锁 |
| L4-4 行锁实现 | 2 天 | 行级共享/排他锁 |
| L4-5 锁等待队列 | 1 天 | 等待中的锁请求 |
| L4-6 死锁检测 (超时) | 1 天 | 简单超时机制 |
| L4-7 锁管理器并发测试 | 1 天 | 多事务并发测试 |

#### 4.4 WAL（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| W4-1 LSN 设计 | 0.5 天 | 日志序列号 |
| W4-2 WAL 日志格式 | 1 天 | 日志记录类型和结构 |
| W4-3 日志写入 | 1 天 | 追加写入日志文件 |
| W4-4 日志刷盘 | 0.5 天 | fsync 确保持久化 |
| W4-5 Checkpoint | 2 天 | 检查点机制 |
| W4-6 Redo 恢复 | 2 天 | 重放日志恢复数据 |
| W4-7 恢复测试 | 1 天 | 模拟崩溃恢复测试 |

**里程碑验证**：支持完整 ACID 事务，崩溃后能恢复

---

### Phase 5: 功能完善（持续演进）

| 功能 | 预计 | 验证标准 |
|------|------|----------|
| ORDER BY | 1 周 | `SELECT * FROM t ORDER BY a` |
| LIMIT/OFFSET | 0.5 周 | `SELECT * FROM t LIMIT 10` |
| 聚合函数 | 2 周 | COUNT/SUM/AVG/MIN/MAX |
| GROUP BY | 1 周 | `SELECT a, COUNT(*) FROM t GROUP BY a` |
| Hash Join | 2 周 | `SELECT * FROM a JOIN b ON a.id = b.id` |
| Nested Loop Join | 1 周 | 小表 join |
| 子查询 | 2 周 | `SELECT * FROM t WHERE a IN (SELECT b FROM s)` |

---

## 4.1 开发节奏建议

- **每周**：完成 3-5 个细粒度任务
- **每个任务**：独立可测试，不超过 2 天
- **每日验证**：当天任务当天测试通过
- **每周回顾**：检查进度，调整计划

---

## 5. SQL 语法支持（Phase 1 范围）

### 5.1 DDL
```sql
CREATE TABLE table_name (
    column_name data_type [NULL | NOT NULL],
    ...
);

DROP TABLE table_name;
```

### 5.2 DML
```sql
INSERT INTO table_name (col1, col2, ...) VALUES (val1, val2, ...);

SELECT col1, col2, ... FROM table_name
WHERE condition
ORDER BY col [ASC | DESC]
LIMIT n;

UPDATE table_name SET col = value WHERE condition;

DELETE FROM table_name WHERE condition;
```

### 5.3 支持的数据类型
- INTEGER (4 bytes)
- BIGINT (8 bytes)
- VARCHAR(n)
- BOOLEAN
- FLOAT / DOUBLE

### 5.4 支持的表达式
- 比较操作: =, <>, <, >, <=, >=
- 逻辑操作: AND, OR, NOT
- 算术操作: +, -, *, /
- NULL 判断: IS NULL, IS NOT NULL

---

## 6. 参考资源

### 6.1 学习资料
- [Database Internals (O'Reilly)](https://www.oreilly.com/library/view/database-internals/9781492040330/) - 存储引擎圣经
- [CMU 15-445 Database Systems](https://15445.courses.cs.cmu.edu/) - 课程项目
- [Let's Build a Simple Database](https://cstack.github.io/db_tutorial/) - SQLite clone 教程

### 6.2 参考源码
- PostgreSQL - 主要参考（协议、SQL 语法、代码风格）
- DuckDB - C++ 现代实践、测试框架
- SQLite - 简洁的 B+ 树实现

### 6.3 关键技术文档
- [SQLite Internals: Pages & B-trees](https://fly.io/blog/sqlite-internals-btree/)
- [PostgreSQL Protocol Documentation](https://www.postgresql.org/docs/current/protocol.html)
- [ARIES Paper](https://cs.stanford.edu/people/csilvers/papers/aries.pdf) - WAL 与恢复

---

## 7. 验证方法

### 7.1 单元测试
- 每个模块都有对应的单元测试
- 使用 Catch2 框架
- 目标覆盖率：核心模块 > 80%

### 7.2 集成测试
- SQLLogicTest 风格的测试用例（参考 DuckDB）
- 端到端的 SQL 查询验证

### 7.3 功能验证
- 使用 psql 客户端连接测试
- 与 PostgreSQL 行为对比验证

### 7.4 性能基准（可选）
- sysbench 风格的基准测试
- 与 SQLite 做简单对比

---

## 8. 项目进度

| Phase | 描述 | 状态 | 完成日期 |
|-------|------|------|----------|
| Phase 0 | 项目搭建、基础设施 | ✅ 完成 | 2026-03-11 |
| Phase 1.1 | 词法分析器 (Lexer) | ✅ 完成 | 2026-03-12 |
| Phase 1.2 | 语法分析器 (Parser) | ✅ 完成 | 2026-03-17 |
| Phase 1.3-1.4 | 内存存储层 + 执行引擎 | 🔄 设计完成，待实现 | - |
| Phase 1.5 | CLI 工具 | 📋 计划中 | - |
| Phase 2 | 多线程 + PostgreSQL 协议 | 📋 计划中 | - |
| Phase 3 | B+ 树存储引擎 | 📋 计划中 | - |
| Phase 4 | 事务系统 | 📋 计划中 | - |
| Phase 5 | 功能完善 | 📋 计划中 | - |

## 9. 下一步行动

1. ~~**立即开始**：创建项目目录结构和 CMake 配置~~ ✅ 已完成
2. ~~**当前目标**：Phase 1.1 - 词法分析器 (Lexer)~~ ✅ 已完成
3. ~~**当前目标**：Phase 1.2 - 语法分析器 (Parser)~~ ✅ 已完成
4. **当前目标**：Phase 1.3 - 内存存储层
5. **第一个里程碑**：Phase 1 完成，能在内存中执行基础 SQL
6. **第二个里程碑**：Phase 2 完成，能用 psql 连接
7. **第三个里程碑**：Phase 3 完成，数据持久化到磁盘
8. **第四个里程碑**：Phase 4 完成，支持完整事务

---

## 附录：关键技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 存储引擎起步 | 内存存储 | 快速验证 SQL 层，后续可替换 |
| 最终存储引擎 | B+ 树 | 与 PG 一致，学习价值高 |
| 并发模型 | 多线程 + 连接池 | 深入理解并发控制复杂性 |
| SQL 兼容 | PostgreSQL | 熟悉的语法，可对照验证 |
| 测试框架 | Catch2 | DuckDB 采用，现代 C++ 风格 |
