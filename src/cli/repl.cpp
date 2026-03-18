#include "cli/repl.h"

#include <iostream>
#include <memory>
#include <sstream>

#include "cli/formatter.h"
#include "parser/lexer.h"
#include "parser/parser.h"

namespace seeddb {
namespace cli {

Repl::Repl(Catalog& catalog, Executor& executor)
    : catalog_(catalog), executor_(executor) {}

void Repl::run() {
    showWelcome();

    running_ = true;
    accumulated_input_.clear();
    in_multiline_ = false;

    std::string line;
    while (running_ && readLine(line)) {
        // Check for meta commands
        if (!in_multiline_ && !line.empty() && line[0] == '\\') {
            if (handleMetaCommand(line.substr(1))) {
                continue;
            }
        }

        // Accumulate input
        if (!accumulated_input_.empty()) {
            accumulated_input_ += " ";
        }
        accumulated_input_ += line;

        // Check if statement is complete (ends with ;)
        if (!accumulated_input_.empty() &&
            accumulated_input_.find_last_of(';') == accumulated_input_.length() - 1) {
            // Remove trailing semicolon for processing
            std::string sql = accumulated_input_.substr(0, accumulated_input_.length() - 1);
            processInput(sql);
            accumulated_input_.clear();
            in_multiline_ = false;
        } else {
            in_multiline_ = true;
        }
    }

    showGoodbye();
}

void Repl::showWelcome() {
    std::cout << "SeedDB - Lightweight Database Kernel\n";
    std::cout << "Type \\? for help, \\q to quit.\n";
    std::cout << std::endl;
}

void Repl::showGoodbye() {
    std::cout << "Bye." << std::endl;
}

bool Repl::readLine(std::string& line) {
    if (in_multiline_) {
        std::cout << "     -> ";
    } else {
        std::cout << "seeddb> ";
    }
    std::cout.flush();

    if (!std::getline(std::cin, line)) {
        // EOF reached
        std::cout << std::endl;
        return false;
    }
    return true;
}

void Repl::processInput(const std::string& input) {
    if (input.empty()) {
        return;
    }

    executeSql(input);
}

bool Repl::handleMetaCommand(const std::string& cmd) {
    if (cmd == "q" || cmd == "quit") {
        running_ = false;
        return true;
    }

    if (cmd == "?" || cmd == "h" || cmd == "help") {
        showHelp();
        return true;
    }

    if (cmd == "dt") {
        listTables();
        return true;
    }

    std::cout << "Invalid command \\" << cmd << ". Try \\? for help." << std::endl;
    return true;
}

void Repl::showHelp() {
    std::cout << "General\n";
    std::cout << "  \\?      Show this help\n";
    std::cout << "  \\h      Show this help\n";
    std::cout << "  \\q      Quit\n";
    std::cout << "\n";
    std::cout << "Informational\n";
    std::cout << "  \\dt     List tables\n";
    std::cout << std::endl;
}

void Repl::listTables() {
    if (catalog_.tableCount() == 0) {
        std::cout << "No tables found." << std::endl;
        return;
    }

    std::cout << "Tables:\n";
    for (const auto& [name, table] : catalog_) {
        (void)table;  // Unused
        std::cout << "  " << name << "\n";
    }
    std::cout << "(" << catalog_.tableCount() << " row" << (catalog_.tableCount() == 1 ? "" : "s") << ")" << std::endl;
}

void Repl::executeSql(const std::string& sql) {
    // Create lexer and parser
    parser::Lexer lexer(sql);
    parser::Parser parser(lexer);

    // Parse the statement
    auto parse_result = parser.parse();
    if (!parse_result.is_ok()) {
        showError(parse_result.error().message());
        return;
    }

    auto stmt = std::move(parse_result.value());

    // Execute based on statement type
    using namespace seeddb::parser;

    if (auto* create_stmt = dynamic_cast<CreateTableStmt*>(stmt.get())) {
        auto result = executor_.execute(*create_stmt);
        if (result.status() == ExecutionResult::Status::OK) {
            std::cout << "CREATE TABLE" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
    else if (auto* drop_stmt = dynamic_cast<DropTableStmt*>(stmt.get())) {
        auto result = executor_.execute(*drop_stmt);
        if (result.status() == ExecutionResult::Status::OK) {
            std::cout << "DROP TABLE" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
    else if (auto* insert_stmt = dynamic_cast<InsertStmt*>(stmt.get())) {
        auto result = executor_.execute(*insert_stmt);
        if (result.status() == ExecutionResult::Status::OK) {
            // For now, just show INSERT 0 1 (oid 0, count 1)
            std::cout << "INSERT 0 1" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
    else if (auto* update_stmt = dynamic_cast<UpdateStmt*>(stmt.get())) {
        auto result = executor_.execute(*update_stmt);
        if (result.status() == ExecutionResult::Status::OK) {
            std::cout << "UPDATE 1" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
    else if (auto* delete_stmt = dynamic_cast<DeleteStmt*>(stmt.get())) {
        auto result = executor_.execute(*delete_stmt);
        if (result.status() == ExecutionResult::Status::OK) {
            std::cout << "DELETE 1" << std::endl;
        } else if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
        }
    }
    else if (auto* select_stmt = dynamic_cast<SelectStmt*>(stmt.get())) {
        auto result = executor_.execute(*select_stmt);
        if (result.status() == ExecutionResult::Status::ERROR) {
            showError(result.errorMessage());
            return;
        }

        // Collect all results
        std::vector<Row> rows;
        if (result.status() == ExecutionResult::Status::OK && result.hasRow()) {
            rows.push_back(result.row());
        }

        // Iterate through remaining rows
        while (executor_.hasNext()) {
            auto next_result = executor_.next();
            if (next_result.status() == ExecutionResult::Status::OK && next_result.hasRow()) {
                rows.push_back(next_result.row());
            }
        }

        // Get schema from the table being queried
        if (select_stmt->fromTable()) {
            const std::string& table_name = select_stmt->fromTable()->name();
            const Table* table = catalog_.getTable(table_name);
            if (table) {
                std::cout << TableFormatter::format(table->schema(), rows);
            } else {
                std::cout << TableFormatter::format(Schema(), rows);
            }
        } else {
            // No FROM clause - create a dummy schema for the result
            Schema schema;
            if (!rows.empty()) {
                // Create column schema based on first row
                std::vector<ColumnSchema> columns;
                for (size_t i = 0; i < rows[0].size(); ++i) {
                    columns.emplace_back("?column?", LogicalType(LogicalTypeId::VARCHAR));
                }
                schema = Schema(columns);
            }
            std::cout << TableFormatter::format(schema, rows);
        }

        executor_.resetQuery();
    }
    else {
        showError("Unsupported statement type");
    }
}

void Repl::showError(const std::string& message) {
    std::cout << "ERROR:  " << message << std::endl;
}

} // namespace cli
} // namespace seeddb
