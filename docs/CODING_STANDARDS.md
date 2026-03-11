# SeedDB C++ 编码规范

> 参考 PostgreSQL 和 DuckDB 的最佳实践，结合项目实际情况制定。

---

## 1. 命名规范

### 1.1 类型命名（PascalCase）

```cpp
// 类、结构体、枚举
class BufferPoolManager { ... };
struct PageHeader { ... };
enum class LogLevel { ... };

// 类型别名
using ObjectId = uint32_t;
using TransactionId = uint64_t;
```

### 1.2 函数与变量命名（snake_case）

```cpp
// 函数
void read_page(PageId page_id);
int get_connection_count() const;

// 局部变量
int page_count = 0;
std::string table_name;

// 成员变量（带下划线后缀）
class Table {
    int column_count_;
    std::string name_;
};
```

### 1.3 常量命名

```cpp
// 编译期常量：全大写下划线
constexpr size_t DEFAULT_PAGE_SIZE = 8192;
constexpr ObjectId INVALID_OBJECT_ID = std::numeric_limits<ObjectId>::max();

// const 常量：snake_case
const std::string default_config_path = "./seeddb.conf";
```

### 1.4 命名空间

```cpp
namespace seeddb {
namespace utils {
    inline std::string trim(const std::string& str);
}
}
```

---

## 2. 内存管理

### 2.1 优先使用智能指针

```cpp
// 推荐：使用智能指针
auto buffer = std::make_unique<PageBuffer>(page_size);
auto manager = std::make_shared<BufferPoolManager>(config);

// 避免：裸指针用于所有权
PageBuffer* buffer = new PageBuffer(page_size);  // ❌
```

### 2.2 智能指针选择

| 场景 | 推荐类型 |
|------|----------|
| 独占所有权 | `std::unique_ptr<T>` |
| 共享所有权 | `std::shared_ptr<T>` |
| 观察者（不拥有） | `T*` 或 `std::weak_ptr<T>` |
| 动态数组 | `std::vector<T>` 或 `std::unique_ptr<T[]>` |

### 2.3 RAII 原则

```cpp
// 推荐：RAII 封装资源
class MappedFile {
public:
    MappedFile(const std::string& path) {
        fd_ = open(path.c_str(), O_RDONLY);
        if (fd_ < 0) throw Error(ErrorCode::FILE_NOT_FOUND, path);
    }
    ~MappedFile() { if (fd_ >= 0) close(fd_); }

    // 禁止拷贝，允许移动
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

private:
    int fd_;
};
```

### 2.4 避免手动内存管理

```cpp
// 推荐
std::vector<int> ids;
ids.reserve(100);
ids.push_back(42);

// 避免
int* ids = new int[100];  // ❌ 需要手动 delete[]
```

---

## 3. 错误处理

### 3.1 使用 Result<T> 模式

参考 DuckDB 的错误处理方式，使用 `Result<T>` 替代异常：

```cpp
// 返回值或错误
Result<Page> BufferPoolManager::get_page(PageId page_id) {
    if (!is_valid_page(page_id)) {
        return Result<Page>::err(ErrorCode::FILE_NOT_FOUND, "Invalid page");
    }
    return Result<Page>::ok(page);
}

// 调用方
auto result = manager->get_page(page_id);
if (!result.ok()) {
    SEEDDB_ERROR(result.error().message());
    return;
}
Page& page = result.value();
```

### 3.2 错误码分类

| 范围 | 类别 |
|------|------|
| 0 | SUCCESS |
| 1-99 | 通用错误（INTERNAL_ERROR, NOT_IMPLEMENTED） |
| 100-199 | 解析错误（SYNTAX_ERROR, INVALID_IDENTIFIER） |
| 200-299 | 语义错误（TABLE_NOT_FOUND, COLUMN_NOT_FOUND） |
| 300-399 | 类型错误（TYPE_ERROR, TYPE_MISMATCH） |
| 400-499 | 约束错误（UNIQUE_VIOLATION, NOT_NULL_VIOLATION） |
| 500-599 | 事务错误（DEADLOCK_DETECTED） |
| 600-699 | I/O 错误（IO_ERROR, FILE_NOT_FOUND） |
| 700-799 | 连接错误（CONNECTION_ERROR） |

### 3.3 异常使用限制

仅在以下情况使用异常：
- 构造函数失败（无法返回 Result）
- 第三方库要求
- 致命错误（程序无法继续运行）

---

## 4. 字符串使用规范

### 4.1 字符串类型选择

| 场景 | 推荐类型 | 原因 |
|------|----------|------|
| 存储字符串 | `std::string` | 拥有所有权 |
| 函数参数（只读） | `std::string_view` | 避免拷贝 |
| 函数参数（需要所有权） | `const std::string&` | 兼容 C++17 |
| 返回值 | `std::string` | 值返回，RVO 优化 |
| 字符串常量 | `const char*` 或 `constexpr` | 编译期常量 |

### 4.2 示例

```cpp
// 推荐：string_view 作为只读参数
bool starts_with(std::string_view str, std::string_view prefix);

// 推荐：值返回
std::string to_upper(std::string str);  // 可以原地修改

// 推荐：常量使用 const char*
constexpr const char* VERSION = "0.1.0";

// 避免：不必要的拷贝
std::string get_name() {
    return name_;  // RVO 会优化，无需 std::move
}
```

### 4.3 char* 使用限制

```cpp
// 允许：字符串常量
constexpr const char* ERROR_MESSAGE = "Internal error";

// 允许：C API 交互
void write_to_file(const char* path);  // 与系统调用交互

// 禁止：用于动态字符串存储
char* name = new char[100];  // ❌ 使用 std::string
```

---

## 5. 注释规范

### 5.1 Doxygen 风格（参考 DuckDB）

```cpp
/// Buffer pool manager - manages page caching and eviction.
/// Thread-safe. Use instance() for singleton access.
class BufferPoolManager {
public:
    /// Get a page from the buffer pool.
    /// @param page_id The page identifier to fetch.
    /// @return The page, or an error if not found.
    Result<Page> get_page(PageId page_id);

    /// Flush all dirty pages to disk.
    /// Must be called before shutdown.
    void flush_all();

private:
    std::unordered_map<PageId, std::unique_ptr<Page>> pages_;
    mutable std::mutex mutex_;
};
```

### 5.2 注释原则

1. **文档注释**：公共 API 必须有 `///` 注释
2. **实现注释**：复杂逻辑使用 `//` 行内注释
3. **TODO 注释**：格式 `// TODO: 描述`

```cpp
// 查找第一个空闲槽位（线性探测）
for (size_t i = 0; i < capacity; ++i) {
    if (slots[i].is_empty()) {
        return i;
    }
}

// TODO: 优化为跳表或 B+ 树索引
```

---

## 6. 代码格式

### 6.1 缩进与空格

- 缩进：4 空格（不用 Tab）
- 大括号：K&R 风格（函数定义另起一行）

```cpp
class Example {
public:
    void method() {
        if (condition) {
            do_something();
        }
    }
};
```

### 6.2 行长度限制

- 最大 120 字符
- 长行在操作符处换行：

```cpp
Result<std::unique_ptr<Table>> Catalog::create_table(
    const std::string& name,
    const std::vector<ColumnDefinition>& columns) {
    // ...
}
```

### 6.3 包含顺序

```cpp
// 1. 对应的头文件
#include "common/buffer_pool.h"

// 2. 项目内头文件
#include "common/config.h"
#include "storage/page.h"

// 3. 第三方库
#include <catch2/catch_test_macros.hpp>

// 4. 标准库
#include <string>
#include <vector>
```

### 6.4 前置声明

优先使用前置声明减少编译依赖：

```cpp
// 在头文件中
namespace seeddb {
class Table;           // 前置声明
struct PageHeader;     // 前置声明

class BufferPool {
    Table* table_;     // 指针/引用可用前置声明
    PageHeader header_;
};
}
```

---

## 7. 文件组织

### 7.1 头文件与源文件位置

```
src/
├── common/                    # 模块目录
│   ├── types.h               # 内部头文件
│   ├── types.cpp             # 与头文件同目录
│   ├── error.h
│   ├── error.cpp
│   ├── string_utils.h        # 仅头文件的工具
│   └── CMakeLists.txt
├── parser/
│   ├── lexer.h
│   ├── lexer.cpp
│   └── ...
└── include/                   # 跨模块公共头文件（可选）
    └── seeddb/
        └── exported_types.h
```

### 7.2 头文件保护

使用 `#ifndef` 风格：

```cpp
#ifndef SEEDDB_COMMON_BUFFER_POOL_H
#define SEEDDB_COMMON_BUFFER_POOL_H

namespace seeddb {
// ...
}

#endif // SEEDDB_COMMON_BUFFER_POOL_H
```

### 7.3 规则

| 文件类型 | 存放位置 |
|----------|----------|
| 模块内部头文件 | `src/模块名/xxx.h` |
| 模块实现文件 | `src/模块名/xxx.cpp` |
| 仅头文件工具 | `src/模块名/xxx_utils.h` |
| 跨模块公共类型 | `src/include/seeddb/`（可选） |

---

## 8. 并发编程

### 8.1 优先使用 atomic

```cpp
class Logger {
    std::atomic<LogLevel> level_{LogLevel::INFO};  // 无锁读取

    void set_level(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
    }

    LogLevel get_level() const {
        return level_.load(std::memory_order_relaxed);
    }
};
```

### 8.2 必须使用 mutex 时

```cpp
class BufferPool {
    mutable std::mutex mutex_;
    std::unordered_map<PageId, Page> pages_;

public:
    Page* get_page(PageId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pages_.find(id);
        return it != pages_.end() ? &it->second : nullptr;
    }
};
```

### 8.3 线程安全注解

在注释中标注线程安全性：

```cpp
/// Thread-safe. All public methods are safe to call from multiple threads.
class BufferPoolManager { ... };

/// NOT thread-safe. Caller must synchronize access.
class PageBuilder { ... };
```

---

## 9. 测试规范

### 9.1 测试文件位置

```
tests/
└── unit/
    └── common/
        ├── test_types.cpp
        ├── test_error.cpp
        └── test_buffer_pool.cpp
```

### 9.2 测试命名

```cpp
TEST_CASE("BufferPoolManager returns error for invalid page", "[buffer_pool]") {
    BufferPoolManager mgr;
    auto result = mgr.get_page(INVALID_PAGE_ID);
    REQUIRE(!result.ok());
    REQUIRE(result.error().code() == ErrorCode::FILE_NOT_FOUND);
}
```

---

## 参考资料

- [PostgreSQL Coding Conventions](https://www.postgresql.org/docs/current/source.html)
- [DuckDB Source Code](https://github.com/duckdb/duckdb)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
