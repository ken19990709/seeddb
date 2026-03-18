## Why

SeedDB has completed its SQL engine (Lexer + Parser) and memory storage layer design. To validate the entire pipeline and enable interactive database usage, a CLI REPL tool is essential. This is the final piece needed to make Phase 1 "runnable" - allowing users to type SQL queries and see results immediately.

## What Changes

- Add a new `seeddb-cli` executable that provides an interactive SQL shell
- Implement a REPL (Read-Eval-Print Loop) for continuous SQL query execution
- Integrate Parser, Storage, and Executor components into a unified execution pipeline
- Add formatted table output for query results
- Provide user-friendly error messages for SQL syntax and execution errors
- Support command history and basic line editing

## Capabilities

### New Capabilities

- `cli-repl`: Interactive command-line interface with REPL loop, SQL execution, result formatting, and error handling

### Modified Capabilities

<!-- No existing capabilities are being modified -->

## Impact

- **New Files**:
  - `src/cli/main.cpp` - CLI entry point and REPL loop
  - `src/cli/repl.h/cpp` - REPL implementation
  - `src/cli/formatter.h/cpp` - Result table formatting
  - `src/cli/CMakeLists.txt` - Build configuration
- **Build System**: Add new `seeddb-cli` target to CMake
- **Dependencies**: May use readline/libedit for line editing (optional)
- **Integration**: Requires completed Executor implementation (Phase 1.3-1.4)
