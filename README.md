# SeedDB 🌱

A lightweight educational database kernel project, designed for learning database internals.

## Overview

SeedDB is a from-scratch implementation of a relational database management system, built to deepen understanding of:

- **SQL Engine**: Parser, optimizer, executor
- **Storage Engine**: B+ tree index, buffer pool, disk management
- **Transaction System**: MVCC, lock manager, WAL, recovery
- **Concurrency**: Multi-threading, connection pooling
- **Protocol**: PostgreSQL wire protocol compatibility

## Features

- PostgreSQL-compatible SQL syntax (subset)
- PostgreSQL wire protocol (v3.0) - works with psql
- ACID transactions with MVCC
- B+ tree indexes
- In-memory and disk-based storage options

## Project Status

✅ **Phase 0 Complete** - Project infrastructure is ready. Now starting Phase 1 (In-memory Storage + Minimal SQL).

## Quick Start

```bash
# Clone the repository
git clone https://github.com/your-username/seeddb.git
cd seeddb

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests
ctest

# Start the server (coming soon)
./seeddb-server
```

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Client Layer                          │
│         (libseeddb, psql, JDBC via PG protocol)         │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   Protocol Layer                         │
│              PostgreSQL Wire Protocol                    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    SQL Engine                            │
│         Parser → Optimizer → Executor                    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  Storage Engine                          │
│     Buffer Pool | B+ Tree | Table Manager               │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                Transaction Manager                       │
│       Lock Manager | MVCC | WAL | Recovery              │
└─────────────────────────────────────────────────────────┘
```

## Development Roadmap

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Project Setup | ✅ Complete |
| 1 | In-memory Storage + Minimal SQL | 🚧 Next |
| 2 | Multi-threading + PostgreSQL Protocol | ⏳ Pending |
| 3 | B+ Tree Storage Engine | ⏳ Pending |
| 4 | Transaction System (ACID) | ⏳ Pending |
| 5 | Feature Enhancement | ⏳ Pending |

### Phase 0 Deliverables (Complete)
- ✅ Git repository with proper .gitignore
- ✅ CMake build system (C++17, warnings-as-errors)
- ✅ Catch2 testing framework integration
- ✅ Basic type definitions (ObjectId, TransactionId, Lsn, PageId, SlotId)
- ✅ Error handling framework with Result<T> monad
- ✅ Thread-safe logger with file output
- ✅ Configuration management with file loading

## Tech Stack

- **Language**: C++17 (future: Rust migration)
- **Build**: CMake 3.16+
- **Testing**: Catch2 v3.x
- **Style**: PostgreSQL coding style

## Project Structure

```
seeddb/
├── CMakeLists.txt          # Root CMake configuration
├── cmake/                  # CMake modules (Catch2)
├── src/
│   └── common/             # Common utilities (Phase 0)
│       ├── types.h         # Base type definitions
│       ├── error.h/cpp     # Error handling + Result<T>
│       ├── logger.h/cpp    # Thread-safe logger
│       └── config.h/cpp    # Configuration management
├── tests/
│   └── unit/common/        # Unit tests for common module
├── docs/                   # Documentation
├── third_party/            # Third-party libraries
└── seeddb.conf             # Sample configuration
```

## References

- [PostgreSQL Source Code](https://github.com/postgres/postgres)
- [DuckDB Source Code](https://github.com/duckdb/duckdb)
- [SQLite Source Code](https://www.sqlite.org/src/doc/trunk/src/)
- [Database Internals (O'Reilly)](https://www.oreilly.com/library/view/database-internals/9781492040330/)
- [CMU 15-445 Database Systems](https://15445.courses.cs.cmu.edu/)

## License

MIT License

## Contributing

This is primarily an educational project. Contributions, discussions, and feedback are welcome!
