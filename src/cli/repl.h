#ifndef SEEDDB_CLI_REPL_H
#define SEEDDB_CLI_REPL_H

#include <functional>
#include <iostream>
#include <string>

#include "executor/executor.h"
#include "storage/catalog.h"

namespace seeddb {
namespace cli {

/// REPL (Read-Eval-Print Loop) for interactive SQL execution.
class Repl {
public:
    /// Constructs a REPL with the given catalog and executor.
    /// @param catalog The database catalog.
    /// @param executor The query executor.
    Repl(Catalog& catalog, Executor& executor);

    /// Runs the REPL loop until user exits.
    void run();

private:
    /// Displays the welcome message.
    void showWelcome();

    /// Displays the exit message.
    void showGoodbye();

    /// Reads a line of input from stdin.
    /// @return true if input was read, false on EOF.
    bool readLine(std::string& line);

    /// Processes accumulated input.
    /// @param input The SQL input to process.
    void processInput(const std::string& input);

    /// Handles meta commands (starting with \).
    /// @param cmd The command string (without the \).
    /// @return true if the command was handled.
    bool handleMetaCommand(const std::string& cmd);

    /// Executes SQL and displays results.
    /// @param sql The SQL statement to execute.
    void executeSql(const std::string& sql);

    /// Displays help information.
    void showHelp();

    /// Lists all tables in the catalog.
    void listTables();

    /// Formats and displays an error message.
    /// @param message The error message.
    void showError(const std::string& message);

    Catalog& catalog_;
    Executor& executor_;

    bool running_ = false;
    bool in_multiline_ = false;
    std::string accumulated_input_;
};

} // namespace cli
} // namespace seeddb

#endif // SEEDDB_CLI_REPL_H
