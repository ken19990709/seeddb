#ifndef SEEDDB_EXECUTOR_EXECUTOR_H
#define SEEDDB_EXECUTOR_EXECUTOR_H

#include <memory>
#include <string>

#include "common/error.h"
#include "storage/row.h"
#include "storage/catalog.h"

// Forward declarations for parser AST types
namespace seeddb {
namespace ast {
    class Statement;
    class SelectStmt;
    class InsertStmt;
    class UpdateStmt;
    class DeleteStmt;
    class CreateStmt;
    class DropStmt;
} // namespace ast
} // namespace seeddb

namespace seeddb {

// =============================================================================
// ExecutionResult Class
// =============================================================================

/// Represents the result of query execution.
/// Can be in one of three states: OK (success with row), ERROR, or EMPTY.
class ExecutionResult {
public:
    // =========================================================================
    // Status Enum
    // =========================================================================

    /// Execution status
    enum class Status {
        OK,     ///< Success with row data
        ERROR,  ///< Execution error
        EMPTY   ///< No rows (empty result)
    };

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor creates an empty result.
    ExecutionResult() : status_(Status::EMPTY) {}

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /// Create a successful result with row data.
    /// @param row The row data.
    /// @return ExecutionResult with OK status.
    static ExecutionResult ok(Row row) {
        return ExecutionResult(Status::OK, std::move(row));
    }

    /// Create an error result.
    /// @param code The error code.
    /// @param message The error message.
    /// @return ExecutionResult with ERROR status.
    static ExecutionResult error(ErrorCode code, std::string message) {
        return ExecutionResult(code, std::move(message));
    }

    /// Create an empty result (no rows).
    /// @return ExecutionResult with EMPTY status.
    static ExecutionResult empty() {
        return ExecutionResult();
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Get the execution status.
    /// @return The status.
    Status status() const { return status_; }

    /// Check if result has row data.
    /// @return true if has row data, false otherwise.
    bool hasRow() const { return has_row_; }

    /// Get the row data.
    /// Undefined behavior if !hasRow().
    /// @return Const reference to the row.
    const Row& row() const { return row_; }

    /// Get the error code.
    /// @return The error code.
    ErrorCode errorCode() const { return error_code_; }

    /// Get the error message.
    /// @return Const reference to the error message.
    const std::string& errorMessage() const { return error_message_; }

private:
    // Private constructor for OK status
    ExecutionResult(Status status, Row row)
        : status_(status), has_row_(true), row_(std::move(row)) {}

    // Private constructor for ERROR status
    ExecutionResult(ErrorCode code, std::string message)
        : status_(Status::ERROR),
          has_row_(false),
          error_code_(code),
          error_message_(std::move(message)) {}

    Status status_;
    bool has_row_ = false;
    Row row_;
    ErrorCode error_code_ = ErrorCode::SUCCESS;
    std::string error_message_;
};

// =============================================================================
// Executor Class Declaration
// =============================================================================

/// Query execution engine.
/// Executes parsed SQL statements against the database catalog.
/// Implementation will be added in later tasks.
class Executor {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Construct an executor with a catalog reference.
    /// @param catalog The database catalog.
    explicit Executor(Catalog& catalog);

    // =========================================================================
    // DDL Execution (Stubs - implementation in later tasks)
    // =========================================================================

    /// Execute a CREATE TABLE statement.
    /// @param stmt The CREATE statement.
    /// @return Execution result.
    ExecutionResult executeCreate(const ast::CreateStmt& stmt);

    /// Execute a DROP TABLE statement.
    /// @param stmt The DROP statement.
    /// @return Execution result.
    ExecutionResult executeDrop(const ast::DropStmt& stmt);

    // =========================================================================
    // DML Execution (Stubs - implementation in later tasks)
    // =========================================================================

    /// Execute an INSERT statement.
    /// @param stmt The INSERT statement.
    /// @return Execution result.
    ExecutionResult executeInsert(const ast::InsertStmt& stmt);

    /// Execute an UPDATE statement.
    /// @param stmt The UPDATE statement.
    /// @return Execution result.
    ExecutionResult executeUpdate(const ast::UpdateStmt& stmt);

    /// Execute a DELETE statement.
    /// @param stmt The DELETE statement.
    /// @return Execution result.
    ExecutionResult executeDelete(const ast::DeleteStmt& stmt);

    // =========================================================================
    // SELECT Execution - Iterator Interface (Stubs - implementation in later tasks)
    // =========================================================================

    /// Prepare a SELECT statement for execution.
    /// @param stmt The SELECT statement.
    /// @return true if prepared successfully, false otherwise.
    bool prepareSelect(const ast::SelectStmt& stmt);

    /// Check if there are more rows to iterate.
    /// @return true if more rows available, false otherwise.
    bool hasNext() const;

    /// Get the next row.
    /// @return Execution result with row data.
    ExecutionResult next();

    /// Reset the current query state.
    void resetQuery();

private:
    Catalog& catalog_;
};

} // namespace seeddb

#endif // SEEDDB_EXECUTOR_EXECUTOR_H
