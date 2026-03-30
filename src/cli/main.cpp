// SeedDB CLI - Main entry point

#include <cstring>
#include <iostream>

#include "cli/repl.h"
#include "common/config.h"
#include "executor/executor.h"
#include "storage/catalog.h"
#include "storage/storage_manager.h"

using namespace seeddb;

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n";
    std::cout << "\n";
    std::cout << "SeedDB - Lightweight Database Kernel\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help     Show this help message\n";
    std::cout << "  -v, --version  Show version information\n";
}

void printVersion() {
    std::cout << "SeedDB 0.1.0\n";
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        }
        std::cerr << "Unknown option: " << argv[i] << "\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

    // Initialize database components
    Config config;
    config.load("seeddb.conf");

    StorageManager storage_mgr(config.data_directory(), config);
    Catalog catalog;
    storage_mgr.load(catalog);

    Executor executor(catalog, &storage_mgr);

    // Run the REPL
    cli::Repl repl(catalog, executor);
    repl.run();

    return 0;
}
