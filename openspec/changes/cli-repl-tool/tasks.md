## 1. Project Setup

- [x] 1.1 Create `src/cli/` directory structure
- [x] 1.2 Create `src/cli/CMakeLists.txt` with seeddb-cli target
- [x] 1.3 Update root `src/CMakeLists.txt` to include cli subdirectory
- [x] 1.4 Verify project builds with empty main.cpp

## 2. Table Formatter

- [x] 2.1 Create `src/cli/formatter.h` with TableFormatter class declaration
- [x] 2.2 Implement column width calculation (max of header/values)
- [x] 2.3 Implement border line generation (+---+---+)
- [x] 2.4 Implement header row formatting (| col1 | col2 |)
- [x] 2.5 Implement data row formatting with Value::toString()
- [x] 2.6 Implement row count footer ((N rows))
- [x] 2.7 Handle NULL value display as "NULL" string
- [x] 2.8 Add unit tests for formatter in `tests/unit/cli/test_formatter.cpp`

## 3. REPL Core

- [x] 3.1 Create `src/cli/repl.h` with Repl class declaration
- [x] 3.2 Implement input reading with std::getline()
- [x] 3.3 Implement multi-line input accumulation (detect semicolon)
- [x] 3.4 Implement prompt switching (seeddb> vs      -> )
- [x] 3.5 Implement EOF/Ctrl+D handling for graceful exit
- [x] 3.6 Add welcome message and exit message

## 4. Meta Commands

- [x] 4.1 Implement meta command detection (starts with \)
- [x] 4.2 Implement \q (quit) command
- [x] 4.3 Implement \? and \h (help) commands
- [x] 4.4 Implement \dt (list tables) command
- [x] 4.5 Handle unknown meta command with error message

## 5. SQL Execution Pipeline

- [x] 5.1 Implement statement type dispatch (dynamic_cast chain)
- [x] 5.2 Implement CREATE TABLE execution and feedback
- [x] 5.3 Implement DROP TABLE execution and feedback
- [x] 5.4 Implement INSERT execution with row count feedback
- [x] 5.5 Implement UPDATE execution with affected row count
- [x] 5.6 Implement DELETE execution with affected row count
- [x] 5.7 Implement SELECT execution with iterator + formatter

## 6. Error Handling

- [x] 6.1 Implement syntax error formatting with line/column pointer
- [x] 6.2 Implement execution error formatting (table not found, etc.)
- [x] 6.3 Extract error location from parser error messages

## 7. Main Entry Point

- [x] 7.1 Create `src/cli/main.cpp` with main() function
- [x] 7.2 Initialize Catalog instance
- [x] 7.3 Initialize Executor with Catalog reference
- [x] 7.4 Initialize and run REPL loop
- [x] 7.5 Add command-line argument parsing (--help, --version)

## 8. Integration Testing

- [x] 8.1 Manual test: CREATE TABLE, INSERT, SELECT flow
- [x] 8.2 Manual test: Multi-line SQL input
- [x] 8.3 Manual test: Error messages display correctly
- [x] 8.4 Manual test: Meta commands (\q, \dt, \?)
- [x] 8.5 Verify build and run from clean checkout
