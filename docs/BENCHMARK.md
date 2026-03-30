# SeedDB Performance Benchmark

## Overview

SeedDB includes a built-in benchmark tool inspired by industry-standard database benchmarks:

| Benchmark | SeedDB Workload | Description |
|-----------|----------------|-------------|
| sysbench oltp_insert | `insert` | Bulk row insertion throughput |
| sysbench oltp_point_select | `point-select` | Point query by primary key |
| sysbench oltp_update_index | `update-index` | UPDATE on indexed column |
| sysbench oltp_update_non_index | `update-non-index` | UPDATE on non-indexed column |
| TPC-H Q6 (simplified) | `range-select` | Range scan with 1% selectivity |
| sysbench oltp_read_write | `mixed-oltp` | Mixed OLTP workload (60R/20U/10I/10D) |
| sysbench oltp_delete | `delete-scan` | DELETE with scan verification |
| - | `full-scan` | Full table sequential scan |

## Quick Start

```bash
# Build
cd build && cmake .. && make -j$(nproc) seeddb_bench

# Run with default 100K rows
./bin/seeddb_bench

# Run with 10K rows (faster)
./bin/seeddb_bench --rows 10000

# Run specific workload
./bin/seeddb_bench --rows 50000 --workload point-select

# Small buffer pool to stress eviction
./bin/seeddb_bench --rows 100000 --buffer 50
```

## CLI Options

| Option | Default | Description |
|--------|---------|-------------|
| `--rows N` | 100000 | Number of rows to insert |
| `--buffer N` | 1024 | Buffer pool frame count |
| `--warmup N` | 0 | Warmup iterations (reserved) |
| `--workload NAME` | all | Run specific workload |
| `--help` | - | Show usage |

### Available Workloads

- `all` (default) — Run all workloads sequentially
- `insert` — Bulk INSERT only
- `point-select` — SELECT with equality on `id`
- `range-select` — SELECT with range condition (id >= A AND id < B)
- `full-scan` — SELECT * without WHERE clause
- `update-index` — UPDATE score column with WHERE id = N
- `update-non-index` — UPDATE name column (VARCHAR) with WHERE id = N
- `delete-scan` — DELETE WHERE category >= 50, then full scan verify
- `mixed-oltp` — 60% SELECT, 20% UPDATE, 10% INSERT, 10% DELETE

## Table Schema

The benchmark creates a table mimicking sysbench's `oltp_table`:

```sql
CREATE TABLE benchmark (
    id        INT,       -- sequential PK (0 .. N-1)
    name      VARCHAR,   -- "user_0" .. "user_N-1"
    score     DOUBLE,    -- id * 0.01
    category  INT        -- id % 100 (0-99)
);
```

## Output Interpretation

### Throughput

- **ops/s** — Operations per second (higher is better)
- **rows/s** — For scan workloads, rows processed per second

### Latency Percentiles

The benchmark reports per-operation latency distribution:

- **avg** — Average latency
- **p50** — Median (50th percentile)
- **p95** — 95th percentile (tail latency)
- **p99** — 99th percentile (worst-case outliers)
- **max** — Maximum observed latency

### Example Output (10K rows, 1024 buffer frames)

```
  INSERT              10000     5.39 ms     1.9M ops/s
  POINT SELECT        10000  7111.91 ms     1.4K ops/s
  RANGE SELECT          100    157.15 ms       636 ops/s
  FULL SCAN               5      5.11 ms       978 ops/s
  UPDATE INDEX        10000  9793.65 ms     1.0K ops/s
  UPDATE NON-INDEX    10000  7382.53 ms     1.4K ops/s
  MIXED OLTP          10000  6889.00 ms     1.5K ops/s
  DELETE SCAN          5000      2.83 ms     1.8M ops/s
```

## Performance Characteristics

Since SeedDB uses a full table scan execution model (no B-tree indexes), query performance scales linearly with table size:

- **INSERT** is fast (~0.4 us/row) since it appends to heap pages
- **Point SELECT** requires a full scan — O(N) per query, not O(log N)
- **UPDATE/DELETE** also require full scans to find matching rows
- **Buffer pool hit rate** is critical for large tables with small buffer pools

### Tuning Tips

1. **Buffer pool size**: Use `--buffer` to control memory usage. A buffer pool of 1024 frames (4 MB) fits ~100K rows comfortably.

2. **Table size**: Use `--rows` to simulate different scales. Start with 10K for quick iterations, then test at 100K or higher.

3. **Eviction stress**: Set `--buffer 10` with `--rows 50000` to force aggressive page eviction and test buffer pool robustness.

## Workload Details

### INSERT

Measures raw write throughput. Each row is inserted individually via the executor. This tests page allocation, row serialization, and buffer pool write path.

### Point SELECT

Pseudo-random point queries on the `id` column. Since SeedDB lacks an index, every point query performs a full table scan. This workload reveals the cost of sequential scanning and predicate evaluation.

### Range SELECT

Range scans selecting 1% of the table (WHERE id >= A AND id < B). 100 queries are executed with different ranges. Tests compound predicate evaluation efficiency.

### Full Scan

Full table scan without any WHERE clause. Measures raw sequential read throughput. 5 iterations are performed to capture variance.

### UPDATE INDEX / UPDATE NON-INDEX

Single-row updates located by `id` (full scan). The "index" variant updates a DOUBLE column; the "non-index" variant updates a VARCHAR column. Compares the cost of updating fixed-width vs. variable-width columns.

### DELETE SCAN

Deletes all rows with category >= 50 (approximately half), then verifies the remaining row count with a full scan. Tests the delete + scan interaction.

### Mixed OLTP

Simulates an OLTP workload with 60% reads, 20% updates, 10% inserts, and 10% deletes. This is the most realistic workload for measuring overall database throughput under mixed conditions.

## Architecture Notes

- Data is stored in a temporary directory and cleaned up after the benchmark
- The persistence check creates a separate table to verify data survives a StorageManager restart
- All timing uses `std::chrono::high_resolution_clock`
- Latency percentiles are computed from sorted per-operation measurements
