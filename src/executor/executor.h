#ifndef SEEDDB_EXECUTOR_EXECUTOR_H
#define SEEDDB_EXECUTOR_EXECUTOR_H

#include <memory>
#include <string>
#include <unordered_map>

#include "common/error.h"
#include "common/logical_type.h"
#include "storage/row.h"
#include "storage/catalog.h"
#include "parser/ast.h"
#include "executor/aggregate.h"

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
class Executor {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Construct an executor with a catalog reference.
    /// @param catalog The database catalog.
    explicit Executor(Catalog& catalog);

    // =========================================================================
    // DDL Execution
    // =========================================================================

    /// Execute a CREATE TABLE statement.
    /// @param stmt The CREATE statement.
    /// @return Execution result.
    ExecutionResult execute(const parser::CreateTableStmt& stmt);

    /// Execute a DROP TABLE statement.
    /// @param stmt The DROP statement.
    /// @return Execution result.
    ExecutionResult execute(const parser::DropTableStmt& stmt);

    // =========================================================================
    // DML Execution (Stubs - implementation in later tasks)
    // =========================================================================

    /// Execute an INSERT statement.
    /// @param stmt The INSERT statement.
    /// @return Execution result.
    ExecutionResult execute(const parser::InsertStmt& stmt);

    /// Execute an UPDATE statement.
    /// @param stmt The UPDATE statement.
    /// @return Execution result.
    ExecutionResult execute(const parser::UpdateStmt& stmt);

    /// Execute a DELETE statement.
    /// @param stmt The DELETE statement.
    /// @return Execution result.
    ExecutionResult execute(const parser::DeleteStmt& stmt);

    /// Execute a SELECT statement (iterator interface).
    /// @param stmt The SELECT statement.
    /// @return Execution result.
    ExecutionResult execute(const parser::SelectStmt& stmt);

    // =========================================================================
    // SELECT Execution - Iterator Interface (Stubs - implementation in later tasks)
    // =========================================================================

    /// Prepare a SELECT statement for execution.
    /// @param stmt The SELECT statement.
    /// @return true if prepared successfully, false otherwise.
    bool prepareSelect(const parser::SelectStmt& stmt);

    /// Check if there are more rows to iterate.
    /// @return true if more rows available, false otherwise.
    bool hasNext() const;

    /// Get the next row.
    /// @return Execution result with row data.
    ExecutionResult next();

    /// Reset the current query state.
    void resetQuery();

private:
    // =========================================================================
    // Helper Methods
    // =========================================================================

    /// Convert parser DataTypeInfo to LogicalType.
    /// @param dti The data type info from parser.
    /// @return The corresponding LogicalType.
    LogicalType toLogicalType(const parser::DataTypeInfo& dti) const;

    /// Evaluate a WHERE clause expression against a row.
    /// @param expr The expression to evaluate.
    /// @param row The row to evaluate against.
    /// @param schema The schema for column lookups.
    /// @return true if the row matches the WHERE clause, false otherwise.
    bool evaluateWhereClause(const parser::Expr* expr, const Row& row, const Schema& schema) const;

    /// Evaluate an expression to get a Value.
    /// @param expr The expression to evaluate.
    /// @param row The row for column references.
    /// @param schema The schema for column lookups.
    /// @return The evaluated Value.
    Value evaluateExpr(const parser::Expr* expr, const Row& row, const Schema& schema) const;

    /// Project columns from a row based on SELECT list.
    /// @param row The source row.
    /// @param schema The source schema.
    /// @param stmt The SELECT statement.
    /// @param alias_map Map of column names to aliases.
    /// @return Projected row.
    Row projectRow(const Row& row, const Schema& schema, const parser::SelectStmt& stmt,
                   std::unordered_map<std::string, std::string>& alias_map) const;

    /// Compare two rows for equality (for DISTINCT).
    /// @param a First row.
    /// @param b Second row.
    /// @return true if rows are equal.
    bool rowsEqual(const Row& a, const Row& b) const;

    /// Compare two values for ordering.
    /// @param a First value.
    /// @param b Second value.
    /// @return negative if a < b, 0 if equal, positive if a > b.
    int compareValues(const Value& a, const Value& b) const;

    /// Match a string against a LIKE pattern.
    /// @param str The string to match.
    /// @param pattern The LIKE pattern (% and _ wildcards).
    /// @return true if the string matches the pattern.
    bool matchLikePattern(const std::string& str, const std::string& pattern) const;

    /// Sort result rows based on ORDER BY clause.
    /// @param stmt The SELECT statement.
    /// @param alias_map Map of column names to aliases.
    /// @param result_schema Schema of result rows.
    void sortResultRows(const parser::SelectStmt& stmt,
                        const std::unordered_map<std::string, std::string>& alias_map,
                        const Schema& result_schema);

    // =========================================================================
    // Aggregate Helper Methods
    // =========================================================================

    /// Check if the SELECT statement contains aggregate functions.
    /// @param stmt The SELECT statement.
    /// @return true if aggregates are present.
    bool hasAggregates(const parser::SelectStmt& stmt) const;

    /// Check if an expression contains aggregate functions.
    /// @param expr The expression to check.
    /// @return true if the expression contains aggregates.
    bool exprHasAggregates(const parser::Expr* expr) const;

    /// Extract group key from a row based on GROUP BY columns.
    /// @param row The source row.
    /// @param schema The source schema.
    /// @param stmt The SELECT statement.
    /// @return The group key.
    GroupKey extractGroupKey(const Row& row, const Schema& schema,
                             const parser::SelectStmt& stmt) const;

    /// Create accumulators for all aggregates in the query.
    /// @param stmt The SELECT statement.
    /// @return Template AggregateState with configured accumulators.
    std::unique_ptr<AggregateState> createAggregateState(const parser::SelectStmt& stmt) const;

    /// Create an accumulator for a specific aggregate expression.
    /// @param agg The aggregate expression.
    /// @return The accumulator.
    std::unique_ptr<AggregateAccumulator> createAccumulator(const parser::AggregateExpr* agg) const;

    /// Collect aggregates from the SELECT list and HAVING clause.
    /// @param stmt The SELECT statement.
    /// @return Vector of aggregate expressions.
    std::vector<const parser::AggregateExpr*> collectAggregates(const parser::SelectStmt& stmt) const;

    /// Collect aggregates from an expression.
    /// @param expr The expression to scan.
    /// @param aggregates Output vector for found aggregates.
    void collectAggregatesFromExpr(const parser::Expr* expr,
                                   std::vector<const parser::AggregateExpr*>& aggregates) const;

    /// Process aggregate query and populate result_rows_.
    /// @param stmt The SELECT statement.
    /// @param schema The source table schema.
    void processAggregateQuery(const parser::SelectStmt& stmt, const Schema& schema);

    /// Evaluate an expression, replacing aggregates with finalized values.
    /// @param expr The expression to evaluate.
    /// @param group_key The group key values.
    /// @param agg_values The finalized aggregate values.
    /// @param aggregates List of aggregate expressions in order.
    /// @param schema The source schema.
    /// @return The evaluated value.
    Value evaluateExprWithAggregates(const parser::Expr* expr,
                                     const GroupKey& group_key,
                                     const std::vector<Value>& agg_values,
                                     const std::vector<const parser::AggregateExpr*>& aggregates,
                                     const parser::SelectStmt& stmt,
                                     const Schema& schema) const;

    // =========================================================================
    // Aggregate Validation Methods
    // =========================================================================

    /// Validate aggregate query constraints.
    /// @param stmt The SELECT statement.
    /// @param schema The source schema.
    /// @return Error result if validation fails, empty result if OK.
    ExecutionResult validateAggregateQuery(const parser::SelectStmt& stmt,
                                           const Schema& schema) const;

    /// Check if a column is in the GROUP BY clause.
    /// @param col_name The column name to check.
    /// @param stmt The SELECT statement.
    /// @return true if the column is in GROUP BY.
    bool isColumnInGroupBy(const std::string& col_name,
                           const parser::SelectStmt& stmt) const;

    /// Validate that non-aggregate expressions only use GROUP BY columns.
    /// @param expr The expression to validate.
    /// @param stmt The SELECT statement.
    /// @param error_msg Output error message if validation fails.
    /// @return true if valid, false otherwise.
    bool validateExprGroupByConstraint(const parser::Expr* expr,
                                       const parser::SelectStmt& stmt,
                                       std::string& error_msg) const;

    /// Validate aggregate function type constraints (e.g., SUM/AVG require numeric).
    /// @param agg The aggregate expression.
    /// @param schema The source schema.
    /// @param error_msg Output error message if validation fails.
    /// @return true if valid, false otherwise.
    bool validateAggregateTypeConstraint(const parser::AggregateExpr* agg,
                                         const Schema& schema,
                                         std::string& error_msg) const;

    // =========================================================================
    // Aggregate Result Schema Methods
    // =========================================================================

    /// Compute the result schema for an aggregate query.
    /// @param stmt The SELECT statement.
    /// @param source_schema The source table schema.
    /// @param alias_map Output map of column aliases.
    /// @return The result schema with proper types and names.
    Schema computeAggregateResultSchema(const parser::SelectStmt& stmt,
                                        const Schema& source_schema,
                                        std::unordered_map<std::string, std::string>& alias_map) const;

    /// Infer the result type for an aggregate function.
    /// @param agg The aggregate expression.
    /// @param source_schema The source table schema.
    /// @return The logical type of the aggregate result.
    LogicalType inferAggregateType(const parser::AggregateExpr* agg,
                                   const Schema& source_schema) const;

    /// Generate a column name for an aggregate expression.
    /// @param agg The aggregate expression.
    /// @return A descriptive column name (e.g., "COUNT(*)", "SUM(amount)").
    std::string generateAggregateName(const parser::AggregateExpr* agg) const;

    // Query state for SELECT iteration
    Table* current_table_ = nullptr;           ///< Current table being queried.
    std::vector<Row> result_rows_;             ///< Result rows (projected, distinct, sorted).
    size_t current_row_index_ = 0;             ///< Current position in result_rows_.

    Catalog& catalog_;
};

} // namespace seeddb

#endif // SEEDDB_EXECUTOR_EXECUTOR_H
