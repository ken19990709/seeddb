## Context

SeedDB is a lightweight database kernel project with completed components:
- **Parser**: Lexer + Parser producing AST (`Result<std::unique_ptr<Stmt>>`)
- **Storage**: Catalog, Table, Schema, Row, Value classes for in-memory storage
- **Executor**: ExecutionResult-based execution engine with iterator model for SELECT

The CLI tool needs to integrate these components into a usable interactive shell, following PostgreSQL-style user experience while keeping the implementation simple for Phase 1.

Current execution flow:
```
SQL String → Lexer → Parser → AST → Executor → ExecutionResult
```

## Goals / Non-Goals

**Goals:**
- Provide an interactive REPL for SQL query execution
- Format query results in readable table format (psql-like)
- Display clear error messages with context
- Support multi-line SQL input (statements ending with `;`)
- Integrate all existing components into a working pipeline

**Non-Goals:**
- Command history persistence (in-memory only for Phase 1)
- Tab completion
- Configuration file support
- Network connectivity (local only)
- Scripting mode / batch execution (defer to Phase 1.5+)

## Decisions

### Decision 1: Simple REPL Loop (No External Dependencies)

**Choice**: Implement basic REPL using standard C++ I/O (iostream)

**Alternatives Considered**:
- GNU Readline: Rich features but adds dependency complexity
- libedit: BSD-licensed alternative, still external dependency
- replxx: Modern C++ library, but overkill for initial version

**Rationale**: Start simple with std::getline(). The core value is validating the execution pipeline. Line editing features can be added later without architectural changes.

### Decision 2: Result Formatter as Separate Module

**Choice**: Create a `TableFormatter` class that takes Schema + Rows → formatted string

**Rationale**:
- Separation of concerns: REPL handles I/O, Formatter handles presentation
- Reusable for future network protocol (PostgreSQL wire format)
- Testable in isolation

**Format Style**:
```
+------+-------+
| id   | name  |
+------+-------+
| 1    | Alice |
| 2    | Bob   |
+------+-------+
(2 rows)
```

### Decision 3: Statement Dispatcher Pattern

**Choice**: Use `std::visit` with statement type variants for clean dispatch

```cpp
// Example dispatch
ExecutionResult result = std::visit([&](auto&& stmt) {
    using T = std::decay_t<decltype(stmt)>;
    if constexpr (std::is_same_v<T, CreateTableStmt>) {
        return executor.execute(stmt);
    }
    // ... other statement types
}, *parsed_stmt);
```

**Rationale**: Type-safe dispatch without manual type checking. Existing AST uses polymorphism via `Stmt` base class, so we'll use dynamic_cast with if-else chain for simplicity.

### Decision 4: Error Display Format

**Choice**: PostgreSQL-style error messages

```
ERROR:  syntax error at or near "FORM"
LINE 1: SELECT * FORM users;
                 ^
```

**Rationale**: Familiar to users, provides actionable context.

### Decision 5: Directory Structure

**Choice**: New `src/cli/` directory

```
src/cli/
├── CMakeLists.txt
├── main.cpp          # Entry point, REPL loop
├── repl.h/cpp        # REPL class
└── formatter.h/cpp   # Result formatting
```

**Rationale**: Keeps CLI logic separate from core database components. Easy to exclude from library builds.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| No line editing → poor UX | Document as Phase 1 limitation; add readline in Phase 1.5 |
| Multi-line input edge cases | Clear documentation; require `;` terminator |
| Large result sets slow output | Add row limit (default 1000); warn user |
| Memory usage with many rows | Streaming output via iterator (already designed in Executor) |

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         CLI (main.cpp)                       │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐  │
│  │   REPL      │───▶│  Executor   │───▶│  Formatter      │  │
│  │ (repl.cpp)  │    │ (existing)  │    │ (formatter.cpp) │  │
│  └─────────────┘    └─────────────┘    └─────────────────┘  │
│         │                  │                    │            │
│         ▼                  ▼                    ▼            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────┐  │
│  │   Parser    │    │   Catalog   │    │   stdout        │  │
│  │ (existing)  │    │ (existing)  │    │                 │  │
│  └─────────────┘    └─────────────┘    └─────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

1. **Input**: User types SQL, terminated by `;`
2. **Lex**: Create Lexer from input string
3. **Parse**: Parser produces `Result<Stmt>`
4. **Execute**: Executor processes statement, returns `ExecutionResult`
5. **Format**: Formatter converts result to table string
6. **Output**: Print to stdout

## Meta Commands (Future)

Reserve `\` prefix for meta commands (psql-style):
- `\q` - quit
- `\d` - describe tables
- `\dt` - list tables

Implementation: Check input prefix before sending to parser.
