# SeedDB

A lightweight educational database kernel.

## Project Overview

SeedDB is a PostgreSQL-inspired database system designed for learning and experimentation. It implements core database components including buffer pool management, query parsing, and transaction processing.

## Build & Test

```bash
mkdir -p build && cd build
cmake .. && make -j$(nproc)
ctest --output-on-failure
```

## Project Structure

```
src/
├── common/          # Core utilities (types, error, config, logger)
├── parser/          # SQL lexer and parser (Phase 1)
├── executor/        # Query execution engine (Phase 2+)
└── storage/         # Buffer pool, page management (Phase 2+)
tests/
└── unit/            # Unit tests (Catch2)
```

## Coding Standards

详见 [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md)

### 快速参考

**命名规范：**
- 类型（类、结构体、枚举）：`PascalCase`
- 函数、变量、常量：`snake_case`
- 成员变量：`snake_case_`（下划线后缀）
- 编译期常量：`UPPER_SNAKE_CASE`

**内存管理：**
- 优先使用智能指针（`std::unique_ptr`、`std::shared_ptr`）
- 使用 RAII 管理资源
- 避免手动 `new`/`delete`

**错误处理：**
- 使用 `Result<T>` 模式，不抛异常
- 返回 `Result<T>::ok(value)` 或 `Result<T>::err(code, message)`

**字符串：**
- 存储：`std::string`
- 只读参数：`std::string_view`
- 常量：`const char*`

**注释：**
- 公共 API：Doxygen 风格 `///`
- 实现细节：`//` 行内注释

**并发：**
- 优先 `std::atomic` 无锁操作
- 必要时使用 `std::mutex` + `std::lock_guard`

## Development Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Project setup, core utilities | ✅ Complete |
| 1 | SQL Lexer | 🔄 Next |
| 2 | SQL Parser | 📋 Planned |
| 3 | Storage engine | 📋 Planned |
| 4 | Query executor | 📋 Planned |
