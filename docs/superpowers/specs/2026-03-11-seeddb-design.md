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

#### 1.3 内存存储层 + 1.4 执行引擎（合并实现）✅ 已完成

> **完成日期**: 2026-03-18

> **设计日期**: 2026-03-17
> **设计原则**: Iterator 执行模型 + 向量化预留 + DuckDB 风格类型系统

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

#### 1.5 命令行工具（1 周）✅ 已完成

> **完成日期**: 2026-03-18

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| C1-1 简单 REPL | 1 天 | 读取用户输入，输出结果 | ✅ |
| C1-2 SQL 执行集成 | 1 天 | REPL 中执行 SQL 并显示结果 | ✅ |
| C1-3 结果格式化输出 | 1 天 | 表格格式输出，对齐正确 | ✅ |
| C1-4 错误提示 | 1 天 | SQL 错误时显示友好提示 | ✅ |

**里程碑验证**：命令行工具能交互式执行 SQL ✅

---

### Phase 2: SQL 功能增强（4-5 周）

> **设计理念**：在进入持久化之前，完善 SQL 功能，使 CLI 工具能支持大部分常见 SQL 语句

#### 2.1 结果集操作（1 周）✅ 已完成

> **完成日期**: 2026-03-19

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| F2-1 ORDER BY 实现 | 1.5 天 | `SELECT * FROM t ORDER BY a DESC, b ASC` | ✅ |
| F2-2 LIMIT/OFFSET 实现 | 0.5 天 | `SELECT * FROM t LIMIT 10 OFFSET 5` | ✅ |
| F2-3 列别名支持 | 0.5 天 | `SELECT a AS alias FROM t` | ✅ |
| F2-4 SELECT DISTINCT | 1 天 | `SELECT DISTINCT a, b FROM t` | ✅ |
| F2-5 表别名支持 | 0.5 天 | `SELECT t.a FROM users t` | ✅ |

#### 2.2 聚合与分组（1 周）✅ 已完成

> **完成日期**: 2026-03-19

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| F2-6 COUNT 聚合 | 1 天 | `SELECT COUNT(*), COUNT(a), COUNT(DISTINCT a) FROM t` | ✅ |
| F2-7 SUM/AVG 聚合 | 0.5 天 | `SELECT SUM(a), AVG(a) FROM t` | ✅ |
| F2-8 MIN/MAX 聚合 | 0.5 天 | `SELECT MIN(a), MAX(a) FROM t` | ✅ |
| F2-9 GROUP BY | 1 天 | `SELECT a, COUNT(*) FROM t GROUP BY a` | ✅ |
| F2-10 HAVING 子句 | 1 天 | `SELECT a, COUNT(*) FROM t GROUP BY a HAVING COUNT(*) > 1` | ✅ |

#### 2.3 表达式增强（1 周）✅ 已完成

> **完成日期**: 2026-03-19

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| F2-11 CASE WHEN 表达式 | 1.5 天 | `SELECT CASE WHEN a > 0 THEN 'positive' ELSE 'negative' END FROM t` | ✅ |
| F2-12 IN 操作符 | 0.5 天 | `SELECT * FROM t WHERE a IN (1, 2, 3)` | ✅ |
| F2-13 NOT IN 操作符 | 0.5 天 | `SELECT * FROM t WHERE a NOT IN (1, 2, 3)` | ✅ |
| F2-14 BETWEEN 操作符 | 0.5 天 | `SELECT * FROM t WHERE a BETWEEN 1 AND 10` | ✅ |
| F2-15 LIKE 模式匹配 | 1 天 | `SELECT * FROM t WHERE name LIKE 'prefix%'` | ✅ |
| F2-16 NULL 处理增强 | 0.5 天 | `COALESCE(a, b, 'default')`, `NULLIF(a, 0)` | ✅ |

#### 2.4 内置函数（1 周）✅ 已完成

> **完成日期**: 2026-03-22
> **详细设计**: [2026-03-22-builtin-functions-design.md](2026-03-22-builtin-functions-design.md)

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| F2-17 字符串函数 | 1.5 天 | `LENGTH`, `UPPER`, `LOWER`, `TRIM`, `SUBSTRING`, `CONCAT` | ✅ |
| F2-18 数学函数 | 1 天 | `ABS`, `ROUND`, `CEIL`, `FLOOR`, `MOD` | ✅ |
| F2-19 类型转换 | 1 天 | `CAST(a AS INTEGER)`, `CAST(b AS VARCHAR)` | 📋 待实现 |
| F2-20 函数框架 | 0.5 天 | 可扩展的函数注册机制 | ✅ |

#### 2.5 基础 JOIN（1 周）✅ 已完成

> **完成日期**: 2026-03-24

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| F2-21 CROSS JOIN | 0.5 天 | `SELECT * FROM a, b` 或 `SELECT * FROM a CROSS JOIN b` | ✅ |
| F2-22 INNER JOIN | 1.5 天 | `SELECT * FROM a INNER JOIN b ON a.id = b.aid` | ✅ |
| F2-23 LEFT JOIN | 1 天 | `SELECT * FROM a LEFT JOIN b ON a.id = b.aid` | 📋 待实现 |
| F2-24 RIGHT JOIN | 0.5 天 | `SELECT * FROM a RIGHT JOIN b ON a.id = b.aid` | 📋 待实现 |
| F2-25 多表 JOIN | 1 天 | `SELECT * FROM a JOIN b ON ... JOIN c ON ...` | ✅ |

**Phase 2 里程碑验证**：
```sql
-- 复杂查询示例
SELECT 
    u.name,
    COUNT(o.id) AS order_count,
    COALESCE(SUM(o.amount), 0) AS total_amount,
    CASE WHEN SUM(o.amount) > 1000 THEN 'VIP' ELSE 'Normal' END AS level
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
WHERE u.status IN ('active', 'premium')
  AND u.name LIKE 'A%'
GROUP BY u.id, u.name
HAVING COUNT(o.id) > 0
ORDER BY total_amount DESC
LIMIT 10;
```

---

### Phase 3: B+ 树存储引擎（8-9 周）

> **设计理念**：先完成页面管理和 Buffer Pool 基础设施，再通过磁盘化查询执行让索引能真正发挥作用，最后实现 B+ 树索引

#### 3.1 页面管理（1 周）✅ 已完成

> **完成日期**: 2026-03-24

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| D3-1 Page ID 设计 | 0.5 天 | 页面唯一标识 (file_id, page_num) | ✅ |
| D3-2 Page Header | 0.5 天 | 64 字节页头：页号、空闲空间、槽位数、LSN、prev/next 指针 | ✅ |
| D3-3 Slotted Page 格式 | 2 天 | PostgreSQL 风格变长数据存储，槽位数组 (pd_lower/pd_upper) | ✅ |
| D3-4 页面读写 | 1 天 | 页面序列化/反序列化 (4KB PAGE_SIZE) | ✅ |
| D3-5 磁盘文件管理 | 1 天 | DiskManager: 文件打开/关闭，页面读写，空闲页复用 | ✅ |

**里程碑验证**：页面管理测试全部通过 (71 test cases) ✅

#### 3.2 Buffer Pool（2 周）✅ 已完成

> **完成日期**: 2026-03-24
> **详细设计**: [2026-03-24-buffer-pool-design.md](2026-03-24-buffer-pool-design.md)

| 任务 | 预计 | 验证标准 | 状态 |
|------|------|----------|------|
| B3-1 Frame 定义 | 0.5 天 | Buffer Pool 中的页帧 (page + metadata + latch) | ✅ |
| B3-2 Buffer Pool 框架 | 1 天 | 固定大小的页面缓存池 (可配置 buffer_pool_size) | ✅ |
| B3-3 页面获取 (FetchPage) | 1 天 | 从磁盘读取页面到内存，Loading 状态防止并发竞争 | ✅ |
| B3-4 页面刷盘 (FlushPage) | 1 天 | 脏页写回磁盘，支持 FlushAll | ✅ |
| B3-5 LRU 替换策略 | 2 天 | InnoDB 风格 midpoint insertion (young/old sublists) | ✅ |
| B3-6 Pin/Unpin 机制 | 1 天 | 原子引用计数，pin_count > 0 防止淘汰 | ✅ |
| B3-7 页面锁 (PageLatch) | 1 天 | shared_mutex 读写锁，RLatch/WLatch API | ✅ |
| B3-8 Buffer Pool 并发测试 | 1 天 | 多线程访问测试通过 (19 test cases) | ✅ |

**里程碑验证**：Buffer Pool 测试全部通过 (237 assertions in 19 test cases) ✅

#### 3.3 磁盘化查询执行（2 周）📋 计划中

> **设计理念**：当前 Executor 直接操作内存中的 Table::rows_ 向量，索引无法发挥 I/O 优化作用。
> 本阶段将查询执行层与 Buffer Pool 集成，实现真正的磁盘化数据访问。

##### 3.3.1 问题分析

当前架构限制：
```
┌─────────────────────────────────────────────────────────────┐
│ 当前架构 (内存化)                                            │
│                                                              │
│   Startup: StorageManager::load()                           │
│            ↓                                                 │
│   All pages → deserialize → Table::rows_ (全部加载到内存)     │
│            ↓                                                 │
│   Executor: for (row : table.rows_) { ... }  (内存迭代)      │
│            ↓                                                 │
│   Mutation: table.insert() / table.update()  (内存修改)      │
│            ↓                                                 │
│   Persist:  checkpoint() → 全表重写磁盘                       │
└─────────────────────────────────────────────────────────────┘
```

目标架构：
```
┌─────────────────────────────────────────────────────────────┐
│ 目标架构 (磁盘化)                                            │
│                                                              │
│   Startup: 仅加载 catalog.meta (表结构)                       │
│            ↓                                                 │
│   Executor: TableIterator → BufferPool::FetchPage()         │
│            ↓                 (按需加载页面)                   │
│   Mutation: 直接修改 Page → mark dirty                        │
│            ↓                                                 │
│   Persist:  BufferPool::FlushAll() (仅脏页写回)               │
└─────────────────────────────────────────────────────────────┘
```

##### 3.3.2 任务分解

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| T3-1 TableIterator 接口设计 | 0.5 天 | 定义 begin/next/end/currentRow 接口 |
| T3-2 HeapTableIterator 实现 | 2 天 | 通过 BufferPool 逐页读取，按 slot 迭代 |
| T3-3 Executor 改造 - SELECT | 1.5 天 | executeSelect 使用 TableIterator 替代 table.rows_ |
| T3-4 Executor 改造 - UPDATE/DELETE | 1.5 天 | 定位修改使用 Iterator，原地更新 Page |
| T3-5 去除全量加载 | 1 天 | StorageManager::load() 仅加载 schema，不加载 rows |
| T3-6 移除 Table::rows_ | 1 天 | Table 类仅保留 schema，无内存行存储 |
| T3-7 增量 INSERT | 1 天 | 直接通过 BufferPool 追加到页面，标记 dirty |
| T3-8 增量 UPDATE/DELETE | 1.5 天 | 原地修改页面记录，无需全表重写 |
| T3-9 集成测试 | 1 天 | 大数据量测试 (10万行+)，内存占用验证 |

**里程碑验证**：
- 插入 10 万行数据，内存占用保持在 buffer_pool_size 范围内
- SELECT/UPDATE/DELETE 通过 BufferPool 执行，不全量加载
- 重启后数据完整性验证

##### 3.3.3 关键设计决策

| 决策点 | 选择 | 原因 |
|--------|------|------|
| Iterator 模式 | Volcano-style (next()) | 与后续向量化兼容，教育价值高 |
| 原地更新策略 | Slot 复用 + Compaction | 简化实现，避免引入复杂的版本链 |
| 删除策略 | 标记删除 + 后台回收 | 延迟回收，减少写放大 |
| Table 类角色 | 仅保留 schema metadata | 移除 rows_ 向量，真正磁盘化 |

#### 3.4 索引目录扩展（0.5 周）📋 计划中

> **设计理念**：为 B+ 树索引准备元数据基础设施

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| C3-1 IndexSchema 定义 | 0.5 天 | 索引名、表名、列列表、唯一性标志 |
| C3-2 Catalog 扩展 | 0.5 天 | createIndex/dropIndex/getIndex API |
| C3-3 catalog.meta 格式扩展 | 0.5 天 | 持久化索引定义 |
| C3-4 CREATE INDEX 解析 | 0.5 天 | Parser 支持 `CREATE INDEX idx ON t(col)` |
| C3-5 DROP INDEX 解析 | 0.5 天 | Parser 支持 `DROP INDEX idx` |

**里程碑验证**：能解析并持久化索引定义，重启后索引元数据完整

#### 3.5 B+ 树（3 周）📋 计划中

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| I3-1 B+ 树节点定义 | 1 天 | InternalNode, LeafNode (基于 Page) |
| I3-2 节点搜索 | 1 天 | 在节点内二分查找键 |
| I3-3 叶子节点插入 | 2 天 | 插入键值对，无分裂 |
| I3-4 节点分裂 | 2 天 | 节点满时分裂，通过 BufferPool 分配新页 |
| I3-5 内部节点插入 | 2 天 | 插入子节点指针 |
| I3-6 B+ 树查找 | 1 天 | 从根到叶子查找，通过 BufferPool 访问 |
| I3-7 B+ 树插入 | 2 天 | 完整插入流程（含分裂） |
| I3-8 B+ 树删除 | 2 天 | 删除 + 合并（简化版） |
| I3-9 范围扫描 | 1 天 | 叶子节点链表遍历 (prev_page/next_page) |
| I3-10 B+ 树并发控制 | 2 天 | crabbing 协议 (利用 PageLatch) |
| I3-11 B+ 树集成测试 | 1 天 | 大量数据插入/查询测试 |

**里程碑验证**：
- 数据持久化到磁盘，重启后数据不丢失
- 索引查找性能优于全表扫描 (10万行表，点查 < 10ms)

---

### Phase 4: 基础恢复机制（2 周）

> **设计理念**：在添加网络协议之前，确保数据持久化和崩溃恢复能力

#### 4.1 WAL 基础（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| W4-1 LSN 设计 | 0.5 天 | 日志序列号生成 |
| W4-2 WAL 日志格式 | 1 天 | 日志记录类型和结构定义 |
| W4-3 日志写入 | 1 天 | 追加写入日志文件 |
| W4-4 日志刷盘 | 0.5 天 | fsync 确保持久化 |
| W4-5 简单 Checkpoint | 1 天 | 定期刷新脏页 |
| W4-6 Redo 恢复 | 2 天 | 重放日志恢复数据 |
| W4-7 恢复测试 | 1 天 | 模拟崩溃恢复测试 |

**里程碑验证**：数据库崩溃后能自动恢复到一致状态

---

### Phase 5: 多线程 + PostgreSQL 协议（4 周）

> **设计理念**：在持久化完成后，添加网络协议支持，使数据库可被标准客户端访问

#### 5.1 线程框架（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| T5-1 线程池实现 | 1 天 | 固定大小线程池，任务提交和执行 |
| T5-2 Mutex 封装 | 0.5 天 | `Mutex`, `LockGuard` RAII 封装 |
| T5-3 ConditionVariable 封装 | 0.5 天 | 条件变量封装 |
| T5-4 线程安全队列 | 1 天 | 生产者-消费者模式测试通过 |
| T5-5 并发测试 | 1 天 | 多线程插入/查询测试通过 |

#### 5.2 连接管理（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| T5-6 Socket 监听 | 1 天 | 监听端口，接受连接 |
| T5-7 Connection 类 | 1 天 | 封装 socket，读写操作 |
| T5-8 Session 类 | 1 天 | 管理会话状态（当前数据库、事务等） |
| T5-9 Connection-Session 映射 | 1 天 | 每个连接对应一个 Session |

#### 5.3 PostgreSQL 协议（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| P5-1 消息读写框架 | 1 天 | 消息格式封装（类型+长度+内容） |
| P5-2 Startup 消息处理 | 1 天 | 解析启动消息，返回认证成功 |
| P5-3 Simple Query 协议 | 2 天 | 处理 Q 消息，返回结果 |
| P5-4 RowDescription 消息 | 1 天 | 描述返回列的类型 |
| P5-5 DataRow 消息 | 1 天 | 返回数据行 |
| P5-6 CommandComplete 消息 | 0.5 天 | 返回命令完成状态 |
| P5-7 ErrorResponse 消息 | 0.5 天 | 返回错误信息 |
| P5-8 协议集成测试 | 1 天 | psql 连接成功，执行查询 |

**里程碑验证**：`psql -h localhost -p 5432` 连接成功，数据持久化

---

### Phase 6: 完整事务系统（4 周）

> **设计理念**：在持久化和网络协议完成后，实现完整的 MVCC 事务支持

#### 6.1 事务框架（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| X6-1 事务 ID 生成器 | 0.5 天 | 全局递增事务 ID |
| X6-2 Transaction 对象 | 1 天 | 事务状态、ID、开始时间戳 |
| X6-3 TransactionManager | 1 天 | BEGIN/COMMIT/ROLLBACK 管理 |
| X6-4 事务状态机 | 1 天 | 状态转换正确 |
| X6-5 事务隔离级别 | 0.5 天 | 支持 READ COMMITTED |

#### 6.2 MVCC（2 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| M6-1 版本链设计 | 1 天 | 每行指向旧版本的指针 |
| M6-2 xmin/xmax 字段 | 1 天 | 创建/删除事务 ID |
| M6-3 ReadView | 2 天 | 快照，判断可见性 |
| M6-4 可见性判断算法 | 2 天 | 正确判断行对事务是否可见 |
| M6-5 版本垃圾回收 (VACUUM) | 1 天 | 清理旧版本（简化版） |

#### 6.3 锁管理器（1 周）

| 任务 | 预计 | 验证标准 |
|------|------|----------|
| L6-1 Lock Request | 0.5 天 | 锁请求对象 |
| L6-2 Lock Manager 框架 | 1 天 | 锁表管理 |
| L6-3 表锁实现 | 1 天 | S锁/X锁 |
| L6-4 行锁实现 | 1 天 | 行级共享/排他锁 |
| L6-5 锁等待与死锁检测 | 1 天 | 等待队列 + 超时机制 |

**里程碑验证**：支持完整 ACID 事务，多客户端并发访问正确

---

### Phase 7: 高级功能（持续演进）

> **2026-03-30 更新**：基于 SQL 完整性差距分析，按投入产出比分阶段推进

#### 7.1 SQL 完整性补全（3-4 周）

> **前置依赖**：Phase 3.3 磁盘化查询执行完成
> **目标**：补齐 SQL 功能短板，覆盖常见业务场景

| 任务 | 预计 | 验证标准 | 优先级 |
|------|------|----------|--------|
| 子查询 (WHERE EXISTS/IN) | 1.5 周 | `SELECT * FROM t WHERE a IN (SELECT b FROM s)` | P0 |
| 子查询 (FROM 子句) | 1 周 | `SELECT * FROM (SELECT a, b FROM t) AS sub` | P0 |
| 子查询 (标量) | 1 周 | `SELECT (SELECT MAX(b) FROM t2) FROM t1` | P0 |
| 约束 (PRIMARY KEY) | 1 周 | 自动创建唯一索引 + 唯一性校验 | P0 |
| 约束 (UNIQUE/DEFAULT/CHECK) | 1 周 | 插入时校验约束、DEFAULT 值填充 | P1 |
| 约束 (FOREIGN KEY) | 1.5 周 | 引用完整性校验 + 级联操作 | P1 |
| CAST() 完整实现 | 0.5 天 | `CAST(a AS INTEGER)`, 隐式转换规则 | P1 |
| ALTER TABLE | 1 周 | ADD/DROP/RENAME COLUMN, 修改数据类型 | P1 |
| VIEW | 1 周 | CREATE VIEW / DROP VIEW, 查询重写 | P1 |
| UNION / INTERSECT / EXCEPT | 1 周 | 含 UNION ALL，集合运算语义正确 | P1 |
| CTE (WITH ... AS) | 1 周 | 非递归 CTE，查询可读性提升 | P1 |

#### 7.2 查询优化与性能（3-4 周）

> **前置依赖**：Phase 3.5 B+ 树索引完成
> **目标**：从"能跑"到"跑得快"

| 任务 | 预计 | 验证标准 | 优先级 |
|------|------|----------|--------|
| Hash JOIN 实现 | 1 周 | 大表 JOIN 性能提升 10x+ | P0 |
| Sort-Merge JOIN 实现 | 1 周 | 有序数据 JOIN 场景优化 | P1 |
| LEFT/RIGHT JOIN 执行完善 | 1 周 | NULL 填充语义正确，含多表外连接 | P0 |
| FULL OUTER JOIN | 0.5 天 | 两侧 NULL 填充 | P2 |
| 索引扫描集成 | 1 周 | WHERE 条件自动选择索引 vs 全表扫描 | P0 |
| EXPLAIN 命令 | 1 周 | 显示查询计划树（访问方式、预估行数） | P1 |
| 基于规则优化器 | 2 周 | 谓词下推、常量折叠、投影裁剪 | P1 |
| 查询结果缓存 | 1 周 | 相同查询直接返回缓存 | P2 |

#### 7.3 分析与高级特性（2-3 周）

> **前置依赖**：7.1 子查询完成
> **目标**：支持分析型查询场景

| 任务 | 预计 | 验证标准 | 优先级 |
|------|------|----------|--------|
| 窗口函数基础 | 2 周 | ROW_NUMBER / RANK / SUM() OVER (ORDER BY / PARTITION BY) | P1 |
| 日期时间类型 | 1 周 | DATE / TIMESTAMP / INTERVAL, NOW() / DATE_ADD 等 | P1 |
| 递归 CTE | 1 周 | WITH RECURSIVE, 层级数据查询 | P2 |
| ANY / ALL 量词 | 0.5 天 | `> ALL (SELECT ...)`, `= ANY (...)` | P2 |

#### 7.4 远期规划

> 无明确时间线，按需推进

| 任务 | 说明 |
|------|------|
| Prepared Statement | 参数化查询 (?占位符)，防 SQL 注入 + 执行计划复用 |
| 向量化执行 | 列式批处理，利用 CPU Cache Line |
| 存储过程 / 函数 | CREATE FUNCTION / CREATE PROCEDURE |
| 触发器 | BEFORE/AFTER INSERT/UPDATE/DELETE |
| JSON 类型 | JSON 解析、路径查询 (jsonb 风格) |
| Array 类型 | PostgreSQL 风格数组 |
| 物化视图 | 自动/手动刷新的缓存结果集 |
| 查询并行 | 并行扫描、并行 JOIN (多线程执行算子) |

---

## 4.1 开发节奏建议

- **每周**：完成 3-5 个细粒度任务
- **每个任务**：独立可测试，不超过 2 天
- **每日验证**：当天任务当天测试通过
- **每周回顾**：检查进度，调整计划

---

## 5. SQL 语法支持

### 5.1 DDL（Phase 1）
```sql
CREATE TABLE table_name (
    column_name data_type [NULL | NOT NULL],
    ...
);

DROP TABLE table_name;
```

### 5.2 DML 基础（Phase 1）
```sql
INSERT INTO table_name (col1, col2, ...) VALUES (val1, val2, ...);

SELECT col1, col2, ... FROM table_name WHERE condition;

UPDATE table_name SET col = value WHERE condition;

DELETE FROM table_name WHERE condition;
```

### 5.3 DML 增强（Phase 2）
```sql
-- 结果集操作
SELECT DISTINCT col1, col2 FROM t ORDER BY col1 DESC, col2 ASC LIMIT 10 OFFSET 5;

-- 聚合与分组
SELECT col1, COUNT(*), SUM(col2), AVG(col2)
FROM t
GROUP BY col1
HAVING COUNT(*) > 1;

-- 表达式
SELECT 
    CASE WHEN a > 0 THEN 'positive' ELSE 'negative' END,
    COALESCE(b, 'default'),
    CAST(c AS INTEGER)
FROM t
WHERE a IN (1, 2, 3) 
  AND b BETWEEN 10 AND 20
  AND name LIKE 'prefix%';

-- JOIN
SELECT a.*, b.col
FROM table_a a
INNER JOIN table_b b ON a.id = b.a_id
LEFT JOIN table_c c ON a.id = c.a_id;
```

### 5.4 支持的数据类型
- INTEGER (4 bytes)
- BIGINT (8 bytes)
- VARCHAR(n)
- BOOLEAN
- FLOAT / DOUBLE

### 5.5 支持的表达式（Phase 1）
- 比较操作: =, <>, <, >, <=, >=
- 逻辑操作: AND, OR, NOT
- 算术操作: +, -, *, /
- NULL 判断: IS NULL, IS NOT NULL

### 5.6 支持的表达式（Phase 2）
- 条件表达式: CASE WHEN ... THEN ... ELSE ... END
- 范围操作: IN, NOT IN, BETWEEN
- 模式匹配: LIKE (支持 % 和 _ 通配符)
- NULL 函数: COALESCE, NULLIF

### 5.7 内置函数（Phase 2）

| 类别 | 函数 |
|------|------|
| **字符串** | LENGTH, UPPER, LOWER, TRIM, SUBSTRING, CONCAT |
| **数学** | ABS, ROUND, CEIL, FLOOR, MOD |
| **类型转换** | CAST |
| **聚合** | COUNT, SUM, AVG, MIN, MAX |

### 5.8 SQL 完整性差距分析（2026-03-30 更新）

> 对比 PostgreSQL/SQLite/MySQL，梳理 SeedDB 当前 SQL 能力边界

#### 5.8.1 已实现能力矩阵

| 特性 | SeedDB | SQLite | PostgreSQL |
|------|--------|--------|------------|
| 基础 DML (CRUD) | ✅ | ✅ | ✅ |
| WHERE 条件 | ✅ | ✅ | ✅ |
| ORDER BY / LIMIT / OFFSET | ✅ | ✅ | ✅ |
| DISTINCT | ✅ | ✅ | ✅ |
| GROUP BY / HAVING / 聚合 | ✅ | ✅ | ✅ |
| CASE WHEN / IN / BETWEEN / LIKE | ✅ | ✅ | ✅ |
| COALESCE / NULLIF | ✅ | ✅ | ✅ |
| INNER / LEFT / RIGHT JOIN (Parser) | ✅ | ✅ | ✅ |
| 内置标量函数 (字符串/数学) | ✅ | ✅ | ✅ |
| NULL 三值逻辑 | ✅ | ✅ | ✅ |
| 列别名 / 表别名 | ✅ | ✅ | ✅ |

#### 5.8.2 缺失功能清单（按优先级排序）

**🔴 P0 — 核心缺失（生产必需）**

| 缺失 | 说明 | PostgreSQL/MySQL 做法 |
|------|------|----------------------|
| 索引结构 | 所有查询均为全表扫描 O(n) | B+Tree/Hash/GiST 多种索引 |
| 事务 (ACID) | 无 BEGIN/COMMIT/ROLLBACK | WAL + 锁/MVCC |
| WAL / 崩溃恢复 | 无持久化保证 | ARIES 风格 redo/undo |
| 约束 | 仅 NOT NULL，无 PK/FK/UNIQUE/DEFAULT/CHECK | 完整约束系统 |
| 子查询 | WHERE/FROM/SELECT 中均不支持嵌套 SELECT | 完整子查询支持 |
| LEFT/RIGHT JOIN 执行 | Parser 支持但 Executor 仅实现 Nested Loop | 多种 JOIN 算法 |

**🟡 P1 — 重要缺失（功能完整性）**

| 缺失 | 说明 |
|------|------|
| Set Operations | UNION / UNION ALL / INTERSECT / EXCEPT |
| CTE (WITH) | Common Table Expressions，复杂查询的组织方式 |
| 窗口函数 | ROW_NUMBER / RANK / SUM() OVER (...) |
| ALTER TABLE | 无法修改已有表结构 (ADD/DROP/RENAME COLUMN) |
| VIEW | CREATE VIEW / DROP VIEW |
| 日期时间类型 | DATE / TIME / TIMESTAMP — 业务系统最常用类型 |
| CAST() 完整实现 | F2-19 待实现，类型转换规则不完整 |
| Hash/Merge Join | 仅 Nested Loop，大表 JOIN 性能差 |
| FULL OUTER JOIN | Parser 和 Executor 均未实现 |

**🟢 P2 — 锦上添花（高级特性）**

| 缺失 | 说明 |
|------|------|
| EXPLAIN / EXPLAIN ANALYZE | 查询计划可视化 |
| Prepared Statement | 参数化查询，防 SQL 注入 |
| 存储过程 / 触发器 | 服务端逻辑 |
| JSON / Array 类型 | 半结构化数据支持 |
| NATURAL JOIN / USING | 简化 JOIN 语法 |
| 递归 CTE | 层级数据查询 |
| ANY / ALL 量词 | `> ALL (SELECT ...)` |
| 逻辑视图 / 物化视图 | 查询复用与性能优化 |

#### 5.8.3 执行器能力评估

| 执行能力 | 状态 | 备注 |
|----------|------|------|
| 全表扫描 | ✅ | 内存迭代模式 |
| WHERE 过滤 | ✅ | 支持复杂布尔表达式 |
| 投影 (列选择) | ✅ | 含表达式计算 |
| 排序 (ORDER BY) | ✅ | 多列 ASC/DESC |
| 去重 (DISTINCT) | ✅ | 基于值比较 |
| 聚合 (GROUP BY) | ✅ | Hash 分组 + HAVING 过滤 |
| 嵌套循环 JOIN | ✅ | 唯一 JOIN 算法 |
| 左/右外连接 | ⚠️ | Parser 支持，Executor 未完整实现 |
| Hash JOIN | ❌ | 大表场景性能关键 |
| Merge JOIN | ❌ | 有序数据场景优化 |
| 子查询执行 | ❌ | 完全不支持 |
| 索引扫描 | ❌ | 依赖 B+ 树实现 |
| 向量化执行 | ❌ | 预留接口，未实现 |

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
| Phase 1.3-1.4 | 内存存储层 + 执行引擎 | ✅ 完成 | 2026-03-18 |
| Phase 1.5 | CLI 工具 (REPL) | ✅ 完成 | 2026-03-18 |
| Phase 2.1 | 结果集操作 (ORDER BY/LIMIT/DISTINCT/别名) | ✅ 完成 | 2026-03-19 |
| Phase 2.2 | 聚合与分组 (COUNT/SUM/AVG/MIN/MAX/GROUP BY/HAVING) | ✅ 完成 | 2026-03-19 |
| Phase 2.3 | 表达式增强 (CASE WHEN/IN/BETWEEN/LIKE/COALESCE/NULLIF) | ✅ 完成 | 2026-03-19 |
| Phase 2.4 | 内置函数 (LENGTH/UPPER/LOWER/TRIM/SUBSTRING/CONCAT/ABS/ROUND/CEIL/FLOOR/MOD) | ✅ 完成 | 2026-03-22 |
| Phase 2.5 | 基础 JOIN 支持 (CROSS/INNER/LEFT/RIGHT) | ✅ 完成 | 2026-03-24 |
| Phase 3.1 | 页面管理 (PageID/PageHeader/SlottedPage/DiskManager) | ✅ 完成 | 2026-03-24 |
| Phase 3.2 | Buffer Pool (LRU/Pin/Unpin/PageLatch) | ✅ 完成 | 2026-03-24 |
| Phase 3.3 | 磁盘化查询执行 (TableIterator/增量持久化) | 📋 计划中 | - |
| Phase 3.4 | 索引目录扩展 (IndexSchema/CREATE INDEX) | 📋 计划中 | - |
| Phase 3.5 | B+ 树索引 | 📋 计划中 | - |
| Phase 4 | 基础恢复机制 (WAL) | 📋 计划中 | - |
| Phase 5 | 多线程 + PostgreSQL 协议 | 📋 计划中 | - |
| Phase 6 | 完整事务系统 (MVCC) | 📋 计划中 | - |
| Phase 7.1 | SQL 完整性补全 (子查询/约束/ALTER/VIEW/CTE) | 📋 计划中 | - |
| Phase 7.2 | 查询优化与性能 (Hash JOIN/索引扫描/EXPLAIN/优化器) | 📋 计划中 | - |
| Phase 7.3 | 分析与高级特性 (窗口函数/日期时间/递归CTE) | 📋 计划中 | - |
| Phase 7.4 | 远期规划 (Prepared Stmt/向量化/JSON/存储过程) | 📋 远期 | - |

## 9. 下一步行动

1. ~~**立即开始**：创建项目目录结构和 CMake 配置~~ ✅ 已完成
2. ~~**当前目标**：Phase 1.1 - 词法分析器 (Lexer)~~ ✅ 已完成
3. ~~**当前目标**：Phase 1.2 - 语法分析器 (Parser)~~ ✅ 已完成
4. ~~**当前目标**：Phase 1.3 - 内存存储层~~ ✅ 已完成
5. ~~**当前目标**：Phase 1.5 - CLI 工具 (REPL)~~ ✅ 已完成
6. ~~**当前目标**：Phase 2.1 - 结果集操作 (ORDER BY/LIMIT/DISTINCT/别名)~~ ✅ 已完成
7. **🎉 里程碑达成**：Phase 1 完成，能在内存中执行基础 SQL ✅
8. **🎉 里程碑达成**：Phase 2 完成，支持复杂 SQL 查询 (聚合/分组/表达式/函数/JOIN) ✅
9. ~~**当前目标**：Phase 2.2 - 聚合与分组 (COUNT/SUM/AVG/MIN/MAX/GROUP BY/HAVING)~~ ✅ 已完成
10. ~~**当前目标**：Phase 2.4 - 内置函数 (字符串/数学/类型转换)~~ ✅ 已完成
11. ~~**当前目标**：Phase 2.5 - 基础 JOIN (CROSS/INNER/LEFT/RIGHT)~~ ✅ 已完成
12. ~~**当前目标**：Phase 3.1 - 页面管理 (PageID/PageHeader/SlottedPage)~~ ✅ 已完成
13. ~~**当前目标**：Phase 3.2 - Buffer Pool (LRU/Pin/Unpin/PageLatch)~~ ✅ 已完成
14. **🎉 里程碑达成**：存储引擎基础设施完成，Buffer Pool 和页面管理就绪 ✅
15. **当前目标**：Phase 3.3 - 磁盘化查询执行 (TableIterator/增量持久化)
16. **下一目标**：Phase 3.4 - 索引目录扩展 (IndexSchema/CREATE INDEX)
17. **下一目标**：Phase 3.5 - B+ 树索引实现
18. **后续目标**：Phase 7.1 - SQL 完整性补全 (子查询/约束/ALTER/VIEW)
19. **后续目标**：Phase 7.2 - 查询优化 (Hash JOIN/索引扫描/EXPLAIN)
20. **后续目标**：Phase 4 - WAL 基础恢复机制
21. **第二个里程碑**：Phase 5 完成，能用 psql 连接
22. **第三个里程碑**：Phase 6 完成，支持完整 ACID 事务

---

## 附录：关键技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 存储引擎起步 | 内存存储 | 快速验证 SQL 层，后续可替换 |
| 最终存储引擎 | B+ 树 | 与 PG 一致，学习价值高 |
| 并发模型 | 多线程 + 连接池 | 深入理解并发控制复杂性 |
| SQL 兼容 | PostgreSQL | 熟悉的语法，可对照验证 |
| 测试框架 | Catch2 | DuckDB 采用，现代 C++ 风格 |
| Phase 7 重构 (2026-03-30) | 按优先级分 4 子阶段 | 基于完整性差距分析，索引→子查询/约束→优化→高级特性 |
| 索引优先于事务 | B+ 树先于 WAL/MVCC | 索引是约束(PK)和性能的基础，对用户体验提升最直接 |
