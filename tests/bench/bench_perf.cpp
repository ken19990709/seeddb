// SeedDB Performance Benchmark
// Inspired by sysbench OLTP and TPC-H simplified patterns.
//
// Usage:
//   ./seeddb_bench [options]
//   Options:
//     --rows N          Table size (default: 100000)
//     --buffer N        Buffer pool frames (default: 1024)
//     --warmup N        Warmup iterations per test (default: 0)
//     --workload NAME   Run specific workload (default: all)
//                       Workloads: insert, point-select, range-select,
//                                  update-index, update-non-index,
//                                  delete-scan, full-scan, mixed-oltp
//     --help            Show usage

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "common/config.h"
#include "executor/executor.h"
#include "parser/ast.h"
#include "storage/catalog.h"
#include "storage/storage_manager.h"

using namespace seeddb;
namespace fs = std::filesystem;
namespace chrono = std::chrono;

// =============================================================================
// Configuration
// =============================================================================

struct BenchConfig {
    int rows = 100000;
    int buffer_frames = 1024;
    int warmup_iters = 0;
    std::string workload = "all";
};

static BenchConfig parse_args(int argc, char* argv[]) {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--rows") && i + 1 < argc) {
            cfg.rows = std::stoi(argv[++i]);
        } else if ((arg == "--buffer") && i + 1 < argc) {
            cfg.buffer_frames = std::stoi(argv[++i]);
        } else if ((arg == "--warmup") && i + 1 < argc) {
            cfg.warmup_iters = std::stoi(argv[++i]);
        } else if ((arg == "--workload") && i + 1 < argc) {
            cfg.workload = argv[++i];
        } else if (arg == "--help") {
            std::cout
                << "Usage: seeddb_bench [options]\n"
                << "Options:\n"
                << "  --rows N          Table size (default: 100000)\n"
                << "  --buffer N        Buffer pool frames (default: 1024)\n"
                << "  --warmup N        Warmup iterations per test (default: 0)\n"
                << "  --workload NAME   Run specific workload (default: all)\n"
                << "                    insert, point-select, range-select,\n"
                << "                    update-index, update-non-index,\n"
                << "                    delete-scan, full-scan, mixed-oltp\n"
                << "  --help            Show this help\n";
            exit(0);
        }
    }
    if (cfg.rows <= 0) cfg.rows = 100000;
    if (cfg.buffer_frames <= 0) cfg.buffer_frames = 1024;
    return cfg;
}

// =============================================================================
// Timer & Statistics
// =============================================================================

struct Timer {
    chrono::high_resolution_clock::time_point start;
    void begin() { start = chrono::high_resolution_clock::now(); }
    double elapsed_us() const {
        auto end = chrono::high_resolution_clock::now();
        return chrono::duration<double, std::micro>(end - start).count();
    }
    double elapsed_ms() const { return elapsed_us() / 1000.0; }
};

struct LatencyStats {
    double avg_us = 0;
    double p50_us = 0;
    double p95_us = 0;
    double p99_us = 0;
    double max_us = 0;
    double min_us = 0;
    double total_ms = 0;
    int count = 0;
};

static LatencyStats compute_stats(std::vector<double>& latencies_us) {
    LatencyStats s;
    s.count = static_cast<int>(latencies_us.size());
    if (s.count == 0) return s;

    std::sort(latencies_us.begin(), latencies_us.end());
    double sum = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0);
    s.avg_us = sum / s.count;
    s.min_us = latencies_us.front();
    s.max_us = latencies_us.back();
    s.p50_us = latencies_us[static_cast<size_t>(s.count * 50 / 100)];
    s.p95_us = latencies_us[static_cast<size_t>(s.count * 95 / 100)];
    s.p99_us = latencies_us[static_cast<size_t>(s.count * 99 / 100)];
    s.total_ms = sum / 1000.0;
    return s;
}

// =============================================================================
// Output formatting
// =============================================================================

static void printBanner(const BenchConfig& cfg) {
    std::cout << "\n";
    std::cout << "+======================================================================+\n";
    std::cout << "|  SeedDB Performance Benchmark                                        |\n";
    std::cout << "+======================================================================+\n";
    std::cout << "  Table rows  : " << cfg.rows << "\n";
    std::cout << "  Buffer pool : " << cfg.buffer_frames << " frames\n";
    std::cout << "  Warmup iters: " << cfg.warmup_iters << "\n";
    std::cout << "  Workload    : " << cfg.workload << "\n\n";
}

static void printWorkloadHeader(const std::string& name, const std::string& desc) {
    std::cout << std::string(72, '-') << "\n";
    std::cout << "  " << name << "\n";
    std::cout << "  " << desc << "\n";
    std::cout << std::string(72, '-') << "\n";
}

static void printThroughput(const std::string& label, int ops, double ms) {
    double qps = (ms > 0) ? ops / (ms / 1000.0) : 0;
    std::cout << "  " << std::left << std::setw(40) << label
              << std::setw(8) << ops << " ops"
              << std::fixed << std::setprecision(2) << std::setw(10) << ms << " ms"
              << std::fixed << std::setprecision(0) << std::setw(10) << qps << " ops/s\n";
}

static void printLatency(const LatencyStats& s) {
    std::cout << "  Latency (us): "
              << "avg=" << std::fixed << std::setprecision(1) << s.avg_us
              << "  p50=" << s.p50_us
              << "  p95=" << s.p95_us
              << "  p99=" << s.p99_us
              << "  max=" << s.max_us << "\n";
}

[[maybe_unused]] static void printSeparator() {
    std::cout << std::string(72, '-') << "\n";
}

// =============================================================================
// Benchmark context — holds StorageManager, Catalog, Executor
// =============================================================================

struct BenchContext {
    std::string dir;
    Config config;
    std::unique_ptr<StorageManager> storage_mgr;
    Catalog catalog;
    std::unique_ptr<Executor> executor;

    explicit BenchContext(const BenchConfig& cfg) {
        dir = fs::temp_directory_path().string() + "/seeddb_bench_" +
              std::to_string(reinterpret_cast<uintptr_t>(this));
        fs::create_directories(dir);
        config.set("buffer_pool_size", std::to_string(cfg.buffer_frames));
        storage_mgr = std::make_unique<StorageManager>(dir, config);
        storage_mgr->load(catalog);
        executor = std::make_unique<Executor>(catalog, storage_mgr.get());
    }

    ~BenchContext() {
        executor.reset();
        storage_mgr.reset();
        fs::remove_all(dir);
    }
};

// =============================================================================
// Table creation — inspired by sysbench oltp_table structure
// =============================================================================

static void createBenchTable(BenchContext& ctx) {
    auto create = std::make_unique<parser::CreateTableStmt>("benchmark");
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "id", parser::DataTypeInfo(parser::DataType::INT)));
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "name", parser::DataTypeInfo(parser::DataType::VARCHAR)));
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "score", parser::DataTypeInfo(parser::DataType::DOUBLE)));
    create->addColumn(std::make_unique<parser::ColumnDef>(
        "category", parser::DataTypeInfo(parser::DataType::INT)));
    auto result = ctx.executor->execute(*create);
    if (result.status() != ExecutionResult::Status::EMPTY) {
        std::cerr << "CREATE TABLE failed: " << result.errorMessage() << "\n";
        exit(1);
    }
}

// =============================================================================
// Workload: INSERT — bulk load throughput
// =============================================================================

static double workload_insert(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: INSERT", "Bulk row insertion (sysbench oltp_insert)");

    Timer timer;
    std::vector<double> latencies;
    latencies.reserve(cfg.rows);

    timer.begin();
    for (int i = 0; i < cfg.rows; ++i) {
        Timer row_timer;
        row_timer.begin();

        auto insert = std::make_unique<parser::InsertStmt>("benchmark");
        insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i)));
        insert->addValues(std::make_unique<parser::LiteralExpr>(
            "user_" + std::to_string(i)));
        insert->addValues(std::make_unique<parser::LiteralExpr>(
            static_cast<double>(i) * 0.01));
        insert->addValues(std::make_unique<parser::LiteralExpr>(
            int64_t(i % 100)));
        auto r = ctx.executor->execute(*insert);
        if (r.status() != ExecutionResult::Status::EMPTY) {
            std::cerr << "INSERT failed at row " << i << ": " << r.errorMessage() << "\n";
            exit(1);
        }
        latencies.push_back(row_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("INSERT", cfg.rows, total_ms);

    // Per-row latency
    if (!latencies.empty()) {
        auto stats = compute_stats(latencies);
        printLatency(stats);
        double per_row_us = total_ms * 1000.0 / cfg.rows;
        std::cout << "  Per-row insert latency: "
                  << std::fixed << std::setprecision(1) << per_row_us << " us\n";
    }

    // Page stats
    uint32_t pages = ctx.storage_mgr->pageCount("benchmark");
    std::cout << "  Pages used: " << pages
              << " (" << std::fixed << std::setprecision(1)
              << static_cast<double>(cfg.rows) / pages << " rows/page)\n";
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: Point SELECT — sysbench oltp_point_select
// =============================================================================

static double workload_point_select(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: POINT SELECT",
                        "SELECT * WHERE id = <random> (sysbench oltp_point_select)");

    const int queries = std::min(cfg.rows, 10000);
    std::vector<double> latencies;
    latencies.reserve(queries);

    Timer timer;
    timer.begin();
    for (int i = 0; i < queries; ++i) {
        int target = (i * 7919) % cfg.rows;  // pseudo-random distribution
        Timer q_timer;
        q_timer.begin();

        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("benchmark"));
        select->setWhere(std::make_unique<parser::BinaryExpr>(
            "=",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(target))));
        if (!ctx.executor->prepareSelect(*select)) {
            std::cerr << "prepareSelect failed\n";
            exit(1);
        }
        int count = 0;
        while (ctx.executor->hasNext()) { ctx.executor->next(); count++; }
        ctx.executor->resetQuery();

        latencies.push_back(q_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("POINT SELECT", queries, total_ms);
    auto stats = compute_stats(latencies);
    printLatency(stats);
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: Range SELECT — simplified TPC-H Q6 style
// =============================================================================

static double workload_range_select(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: RANGE SELECT",
                        "SELECT * WHERE id >= A AND id < B (range scan)");

    const int queries = 100;
    const int range_size = cfg.rows / 100;  // 1% selectivity
    std::vector<double> latencies;
    latencies.reserve(queries);

    Timer timer;
    timer.begin();
    for (int i = 0; i < queries; ++i) {
        int lo = (i * range_size) % (cfg.rows - range_size);
        int hi = lo + range_size;

        Timer q_timer;
        q_timer.begin();

        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("benchmark"));
        auto where = std::make_unique<parser::BinaryExpr>(
            "AND",
            std::make_unique<parser::BinaryExpr>(
                ">=",
                std::make_unique<parser::ColumnRef>("id"),
                std::make_unique<parser::LiteralExpr>(int64_t(lo))),
            std::make_unique<parser::BinaryExpr>(
                "<",
                std::make_unique<parser::ColumnRef>("id"),
                std::make_unique<parser::LiteralExpr>(int64_t(hi))));
        select->setWhere(std::move(where));

        if (!ctx.executor->prepareSelect(*select)) {
            std::cerr << "prepareSelect failed\n";
            exit(1);
        }
        int count = 0;
        while (ctx.executor->hasNext()) { ctx.executor->next(); count++; }
        ctx.executor->resetQuery();

        latencies.push_back(q_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("RANGE SELECT (1% selectivity)", queries, total_ms);
    auto stats = compute_stats(latencies);
    printLatency(stats);
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: Full Table Scan — sequential read throughput
// =============================================================================

static double workload_full_scan(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: FULL SCAN",
                        "SELECT * without WHERE (sequential read throughput)");

    const int iterations = 5;
    std::vector<double> latencies;
    latencies.reserve(iterations);

    Timer timer;
    timer.begin();
    for (int i = 0; i < iterations; ++i) {
        Timer q_timer;
        q_timer.begin();

        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("benchmark"));
        if (!ctx.executor->prepareSelect(*select)) {
            std::cerr << "prepareSelect failed\n";
            exit(1);
        }
        int count = 0;
        while (ctx.executor->hasNext()) { ctx.executor->next(); count++; }
        ctx.executor->resetQuery();

        latencies.push_back(q_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("FULL SCAN (x5)", iterations, total_ms);
    auto stats = compute_stats(latencies);
    printLatency(stats);
    double scan_ms = total_ms / iterations;
    double scan_qps = (scan_ms > 0) ? cfg.rows / (scan_ms / 1000.0) : 0;
    std::cout << "  Avg scan throughput: "
              << std::fixed << std::setprecision(0) << scan_qps << " rows/s\n";
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: UPDATE on indexed column (id) — sysbench oltp_update_index
// =============================================================================

static double workload_update_index(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: UPDATE INDEX",
                        "UPDATE score WHERE id = <random> (sysbench oltp_update_index)");

    const int queries = std::min(cfg.rows, 10000);
    std::vector<double> latencies;
    latencies.reserve(queries);

    Timer timer;
    timer.begin();
    for (int i = 0; i < queries; ++i) {
        int target = (i * 7919) % cfg.rows;
        Timer q_timer;
        q_timer.begin();

        auto update = std::make_unique<parser::UpdateStmt>("benchmark");
        update->addAssignment("score",
            std::make_unique<parser::LiteralExpr>(static_cast<double>(i) * 0.001));
        update->setWhere(std::make_unique<parser::BinaryExpr>(
            "=",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(target))));
        auto r = ctx.executor->execute(*update);
        if (r.status() != ExecutionResult::Status::EMPTY) {
            std::cerr << "UPDATE failed: " << r.errorMessage() << "\n";
            exit(1);
        }
        latencies.push_back(q_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("UPDATE INDEX", queries, total_ms);
    auto stats = compute_stats(latencies);
    printLatency(stats);
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: UPDATE on non-indexed column — sysbench oltp_update_non_index
// =============================================================================

static double workload_update_non_index(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: UPDATE NON-INDEX",
                        "UPDATE name WHERE id = <random> (sysbench oltp_update_non_index)");

    const int queries = std::min(cfg.rows, 10000);
    std::vector<double> latencies;
    latencies.reserve(queries);

    Timer timer;
    timer.begin();
    for (int i = 0; i < queries; ++i) {
        int target = (i * 7919) % cfg.rows;
        Timer q_timer;
        q_timer.begin();

        auto update = std::make_unique<parser::UpdateStmt>("benchmark");
        update->addAssignment("name",
            std::make_unique<parser::LiteralExpr>("upd_" + std::to_string(i)));
        update->setWhere(std::make_unique<parser::BinaryExpr>(
            "=",
            std::make_unique<parser::ColumnRef>("id"),
            std::make_unique<parser::LiteralExpr>(int64_t(target))));
        auto r = ctx.executor->execute(*update);
        if (r.status() != ExecutionResult::Status::EMPTY) {
            std::cerr << "UPDATE failed: " << r.errorMessage() << "\n";
            exit(1);
        }
        latencies.push_back(q_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("UPDATE NON-INDEX", queries, total_ms);
    auto stats = compute_stats(latencies);
    printLatency(stats);
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: DELETE + scan verification — sysbench oltp_delete
// =============================================================================

static double workload_delete_scan(BenchContext& ctx, const BenchConfig& /*cfg*/) {
    printWorkloadHeader("Workload: DELETE SCAN",
                        "DELETE WHERE category >= 50, then verify with full scan");

    Timer timer;
    timer.begin();

    // Count before
    int before = 0;
    {
        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("benchmark"));
        ctx.executor->prepareSelect(*select);
        while (ctx.executor->hasNext()) { ctx.executor->next(); before++; }
        ctx.executor->resetQuery();
    }

    // Delete category >= 50
    {
        auto del = std::make_unique<parser::DeleteStmt>("benchmark");
        del->setWhere(std::make_unique<parser::BinaryExpr>(
            ">=",
            std::make_unique<parser::ColumnRef>("category"),
            std::make_unique<parser::LiteralExpr>(int64_t(50))));
        auto r = ctx.executor->execute(*del);
        if (r.status() != ExecutionResult::Status::EMPTY) {
            std::cerr << "DELETE failed: " << r.errorMessage() << "\n";
            exit(1);
        }
    }

    // Verify after
    int after = 0;
    {
        auto select = std::make_unique<parser::SelectStmt>();
        select->setSelectAll(true);
        select->setFromTable(std::make_unique<parser::TableRef>("benchmark"));
        ctx.executor->prepareSelect(*select);
        while (ctx.executor->hasNext()) { ctx.executor->next(); after++; }
        ctx.executor->resetQuery();
    }
    double total_ms = timer.elapsed_ms();

    int deleted = before - after;
    printThroughput("DELETE + VERIFY", deleted, total_ms);
    std::cout << "  Before: " << before << " rows  |  After: " << after << " rows  |  Deleted: " << deleted << "\n";
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Workload: Mixed OLTP — simulates sysbench oltp_read_write
// =============================================================================

static double workload_mixed_oltp(BenchContext& ctx, const BenchConfig& cfg) {
    printWorkloadHeader("Workload: MIXED OLTP",
                        "Mixed read/write (60% SELECT, 20% UPDATE, 10% INSERT, 10% DELETE)");

    const int total_ops = std::min(cfg.rows, 20000);
    std::vector<double> latencies;
    latencies.reserve(total_ops);

    int selects = 0, updates = 0, inserts = 0, deletes = 0;

    Timer timer;
    timer.begin();
    for (int i = 0; i < total_ops; ++i) {
        Timer q_timer;
        q_timer.begin();

        int op_type = i % 10;  // deterministic distribution
        if (op_type < 6) {
            // SELECT (60%)
            int target = (i * 7919) % cfg.rows;
            auto select = std::make_unique<parser::SelectStmt>();
            select->setSelectAll(true);
            select->setFromTable(std::make_unique<parser::TableRef>("benchmark"));
            select->setWhere(std::make_unique<parser::BinaryExpr>(
                "=",
                std::make_unique<parser::ColumnRef>("id"),
                std::make_unique<parser::LiteralExpr>(int64_t(target))));
            ctx.executor->prepareSelect(*select);
            while (ctx.executor->hasNext()) { ctx.executor->next(); }
            ctx.executor->resetQuery();
            selects++;
        } else if (op_type < 8) {
            // UPDATE (20%)
            int target = (i * 7919) % cfg.rows;
            auto update = std::make_unique<parser::UpdateStmt>("benchmark");
            update->addAssignment("score",
                std::make_unique<parser::LiteralExpr>(static_cast<double>(i) * 0.001));
            update->setWhere(std::make_unique<parser::BinaryExpr>(
                "=",
                std::make_unique<parser::ColumnRef>("id"),
                std::make_unique<parser::LiteralExpr>(int64_t(target))));
            ctx.executor->execute(*update);
            updates++;
        } else if (op_type < 9) {
            // INSERT (10%)
            int new_id = cfg.rows + i;
            auto insert = std::make_unique<parser::InsertStmt>("benchmark");
            insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(new_id)));
            insert->addValues(std::make_unique<parser::LiteralExpr>("mix_" + std::to_string(i)));
            insert->addValues(std::make_unique<parser::LiteralExpr>(static_cast<double>(i) * 0.1));
            insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i % 100)));
            ctx.executor->execute(*insert);
            inserts++;
        } else {
            // DELETE + re-insert to avoid shrinking dataset (10%)
            int target = cfg.rows + i - 1;
            auto del = std::make_unique<parser::DeleteStmt>("benchmark");
            del->setWhere(std::make_unique<parser::BinaryExpr>(
                "=",
                std::make_unique<parser::ColumnRef>("id"),
                std::make_unique<parser::LiteralExpr>(int64_t(target))));
            ctx.executor->execute(*del);
            // Re-insert same row to keep dataset stable
            auto insert = std::make_unique<parser::InsertStmt>("benchmark");
            insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(target)));
            insert->addValues(std::make_unique<parser::LiteralExpr>("ri_" + std::to_string(i)));
            insert->addValues(std::make_unique<parser::LiteralExpr>(static_cast<double>(i) * 0.1));
            insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i % 100)));
            ctx.executor->execute(*insert);
            deletes++;
        }
        latencies.push_back(q_timer.elapsed_us());
    }
    double total_ms = timer.elapsed_ms();

    printThroughput("MIXED OLTP", total_ops, total_ms);
    auto stats = compute_stats(latencies);
    printLatency(stats);
    std::cout << "  Breakdown: SELECT=" << selects
              << " UPDATE=" << updates
              << " INSERT=" << inserts
              << " DELETE=" << deletes << "\n";
    std::cout << "\n";
    return total_ms;
}

// =============================================================================
// Persistence verification
// =============================================================================

// Persistence check is done inline in main()

// =============================================================================
// Summary table
// =============================================================================

struct WorkloadResult {
    std::string name;
    int ops;
    double ms;
    double qps;
};

static void printSummary(const std::vector<WorkloadResult>& results) {
    std::cout << "\n";
    std::cout << "+======================================================================+\n";
    std::cout << "|  Summary                                                             |\n";
    std::cout << "+======================================================================+\n\n";

    std::cout << "  " << std::left
              << std::setw(30) << "Workload"
              << std::setw(10) << "Ops"
              << std::setw(12) << "Time(ms)"
              << std::setw(14) << "Throughput"
              << "\n";
    std::cout << "  " << std::string(66, '-') << "\n";

    for (const auto& r : results) {
        std::ostringstream qps_str;
        if (r.qps >= 1000000) {
            qps_str << std::fixed << std::setprecision(1) << r.qps / 1000000.0 << "M ops/s";
        } else if (r.qps >= 1000) {
            qps_str << std::fixed << std::setprecision(1) << r.qps / 1000.0 << "K ops/s";
        } else {
            qps_str << std::fixed << std::setprecision(0) << r.qps << " ops/s";
        }

        std::cout << "  " << std::left
                  << std::setw(30) << r.name
                  << std::setw(10) << r.ops
                  << std::fixed << std::setprecision(2) << std::setw(12) << r.ms
                  << qps_str.str() << "\n";
    }
    std::cout << "\n";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    BenchConfig cfg = parse_args(argc, argv);
    printBanner(cfg);

    std::vector<WorkloadResult> results;

    {
        BenchContext ctx(cfg);
        Timer total_timer;
        total_timer.begin();

        // Phase 1: Create table
        createBenchTable(ctx);

        // Phase 2: INSERT workload (also populates data for other workloads)
        bool run_all = (cfg.workload == "all");
        bool do_insert = run_all || cfg.workload == "insert";

        if (do_insert) {
            double ms = workload_insert(ctx, cfg);
            results.push_back({"INSERT", cfg.rows, ms, (ms > 0) ? cfg.rows / (ms / 1000.0) : 0});
        } else {
            // If not running insert workload, still populate data for other tests
            for (int i = 0; i < cfg.rows; ++i) {
                auto insert = std::make_unique<parser::InsertStmt>("benchmark");
                insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i)));
                insert->addValues(std::make_unique<parser::LiteralExpr>("user_" + std::to_string(i)));
                insert->addValues(std::make_unique<parser::LiteralExpr>(static_cast<double>(i) * 0.01));
                insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i % 100)));
                ctx.executor->execute(*insert);
            }
        }

        // Phase 3: Read/Write workloads
        if (run_all || cfg.workload == "point-select") {
            double ms = workload_point_select(ctx, cfg);
            int ops = std::min(cfg.rows, 10000);
            results.push_back({"POINT SELECT", ops, ms, (ms > 0) ? ops / (ms / 1000.0) : 0});
        }

        if (run_all || cfg.workload == "range-select") {
            double ms = workload_range_select(ctx, cfg);
            results.push_back({"RANGE SELECT", 100, ms, (ms > 0) ? 100 / (ms / 1000.0) : 0});
        }

        if (run_all || cfg.workload == "full-scan") {
            double ms = workload_full_scan(ctx, cfg);
            results.push_back({"FULL SCAN", 5, ms, (ms > 0) ? 5 / (ms / 1000.0) : 0});
        }

        if (run_all || cfg.workload == "update-index") {
            double ms = workload_update_index(ctx, cfg);
            int ops = std::min(cfg.rows, 10000);
            results.push_back({"UPDATE INDEX", ops, ms, (ms > 0) ? ops / (ms / 1000.0) : 0});
        }

        if (run_all || cfg.workload == "update-non-index") {
            double ms = workload_update_non_index(ctx, cfg);
            int ops = std::min(cfg.rows, 10000);
            results.push_back({"UPDATE NON-INDEX", ops, ms, (ms > 0) ? ops / (ms / 1000.0) : 0});
        }

        if (run_all || cfg.workload == "mixed-oltp") {
            double ms = workload_mixed_oltp(ctx, cfg);
            int ops = std::min(cfg.rows, 20000);
            results.push_back({"MIXED OLTP", ops, ms, (ms > 0) ? ops / (ms / 1000.0) : 0});
        }

        // Phase 4: DELETE (destructive, run last)
        if (run_all || cfg.workload == "delete-scan") {
            double ms = workload_delete_scan(ctx, cfg);
            // We don't know exact deleted count here, approximate
            results.push_back({"DELETE SCAN", cfg.rows / 2, ms, (ms > 0) ? (cfg.rows / 2) / (ms / 1000.0) : 0});
        }

        // Phase 5: Persistence check
        if (run_all) {
            printWorkloadHeader("Persistence Check",
                                "Reopen StorageManager and verify data survives restart");

            Timer timer;
            timer.begin();
            // Data still in ctx, need to flush first by destroying and recreating
            std::string persist_dir = ctx.dir + "_persist";
            fs::create_directories(persist_dir);

            // Create separate context to test persistence
            {
                Config config2;
                config2.set("buffer_pool_size", std::to_string(cfg.buffer_frames));
                StorageManager sm_save(persist_dir, config2);
                Catalog cat_save;
                sm_save.load(cat_save);
                Executor exec_save(cat_save, &sm_save);

                auto create = std::make_unique<parser::CreateTableStmt>("persist_test");
                create->addColumn(std::make_unique<parser::ColumnDef>(
                    "id", parser::DataTypeInfo(parser::DataType::INT)));
                create->addColumn(std::make_unique<parser::ColumnDef>(
                    "data", parser::DataTypeInfo(parser::DataType::VARCHAR)));
                exec_save.execute(*create);

                for (int i = 0; i < 1000; ++i) {
                    auto insert = std::make_unique<parser::InsertStmt>("persist_test");
                    insert->addValues(std::make_unique<parser::LiteralExpr>(int64_t(i)));
                    insert->addValues(std::make_unique<parser::LiteralExpr>("data_" + std::to_string(i)));
                    exec_save.execute(*insert);
                }
                // sm_save destructor flushes dirty pages
            }

            // Session 2: Read back
            {
                Config config2;
                config2.set("buffer_pool_size", std::to_string(cfg.buffer_frames));
                StorageManager sm_load(persist_dir, config2);
                Catalog cat_load;
                sm_load.load(cat_load);
                Executor exec_load(cat_load, &sm_load);

                auto select = std::make_unique<parser::SelectStmt>();
                select->setSelectAll(true);
                select->setFromTable(std::make_unique<parser::TableRef>("persist_test"));
                exec_load.prepareSelect(*select);
                int count = 0;
                while (exec_load.hasNext()) {
                    auto r = exec_load.next();
                    int id = r.row().get(0).asInt32();
                    std::string data = r.row().get(1).asString();
                    if (data != "data_" + std::to_string(id)) {
                        std::cerr << "Data mismatch at id=" << id << "\n";
                        exit(1);
                    }
                    count++;
                }
                exec_load.resetQuery();
                double ms = timer.elapsed_ms();
                printThroughput("PERSIST + VERIFY (1000 rows)", count, ms);
                std::cout << "  All " << count << " rows verified intact.\n\n";
                if (count != 1000) {
                    std::cerr << "Expected 1000 rows, got " << count << "\n";
                    exit(1);
                }
            }
            fs::remove_all(persist_dir);
        }

        double total_ms = total_timer.elapsed_ms();
        std::cout << "  Total benchmark time: "
                  << std::fixed << std::setprecision(2) << total_ms << " ms\n\n";
    }

    // Print summary
    printSummary(results);

    std::cout << "Benchmark complete.\n\n";
    return 0;
}
