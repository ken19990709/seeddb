#include "executor/executor.h"

#include "storage/schema.h"

namespace seeddb {

// =============================================================================
// Constructor
// =============================================================================

Executor::Executor(Catalog& catalog) : catalog_(catalog) {}

// =============================================================================
// DDL Execution
// =============================================================================

ExecutionResult Executor::execute(const parser::CreateTableStmt& stmt) {
    const std::string& table_name = stmt.tableName();

    // Check if table already exists
    if (catalog_.hasTable(table_name)) {
        return ExecutionResult::error(
            ErrorCode::DUPLICATE_TABLE,
            "Table '" + table_name + "' already exists"
        );
    }

    // Build ColumnSchema vector from AST
    std::vector<ColumnSchema> columns;
    for (const auto& col_def : stmt.columns()) {
        LogicalType logical_type = toLogicalType(col_def->dataType());
        columns.emplace_back(
            col_def->name(),
            logical_type,
            col_def->isNullable()
        );
    }

    // Create schema
    Schema schema(std::move(columns));

    // Create table in catalog
    if (!catalog_.createTable(table_name, std::move(schema))) {
        return ExecutionResult::error(
            ErrorCode::DUPLICATE_TABLE,
            "Table '" + table_name + "' already exists"
        );
    }

    return ExecutionResult::empty();
}

ExecutionResult Executor::execute(const parser::DropTableStmt& stmt) {
    const std::string& table_name = stmt.tableName();

    // Check if table exists
    if (!catalog_.hasTable(table_name)) {
        // If IF EXISTS flag is set, return success silently
        if (stmt.hasIfExists()) {
            return ExecutionResult::empty();
        }
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + table_name + "' not found"
        );
    }

    // Drop the table
    catalog_.dropTable(table_name);

    return ExecutionResult::empty();
}

// =============================================================================
// DML Execution (Stubs)
// =============================================================================

ExecutionResult Executor::execute(const parser::InsertStmt& stmt) {
    (void)stmt;  // Suppress unused parameter warning
    return ExecutionResult::error(
        ErrorCode::NOT_IMPLEMENTED,
        "INSERT not implemented"
    );
}

ExecutionResult Executor::execute(const parser::UpdateStmt& stmt) {
    (void)stmt;  // Suppress unused parameter warning
    return ExecutionResult::error(
        ErrorCode::NOT_IMPLEMENTED,
        "UPDATE not implemented"
    );
}

ExecutionResult Executor::execute(const parser::DeleteStmt& stmt) {
    (void)stmt;  // Suppress unused parameter warning
    return ExecutionResult::error(
        ErrorCode::NOT_IMPLEMENTED,
        "DELETE not implemented"
    );
}

ExecutionResult Executor::execute(const parser::SelectStmt& stmt) {
    (void)stmt;  // Suppress unused parameter warning
    return ExecutionResult::error(
        ErrorCode::NOT_IMPLEMENTED,
        "SELECT not implemented"
    );
}

// =============================================================================
// SELECT Execution - Iterator Interface (Stubs)
// =============================================================================

bool Executor::prepareSelect(const parser::SelectStmt& stmt) {
    (void)stmt;  // Suppress unused parameter warning
    return false;
}

bool Executor::hasNext() const {
    return false;
}

ExecutionResult Executor::next() {
    return ExecutionResult::error(
        ErrorCode::NOT_IMPLEMENTED,
        "No rows available"
    );
}

void Executor::resetQuery() {
    // No-op for now
}

// =============================================================================
// Helper Methods
// =============================================================================

LogicalType Executor::toLogicalType(const parser::DataTypeInfo& dti) const {
    switch (dti.base_type_) {
        case parser::DataType::INT:
            return LogicalType(LogicalTypeId::INTEGER);
        case parser::DataType::BIGINT:
            return LogicalType(LogicalTypeId::BIGINT);
        case parser::DataType::FLOAT:
            return LogicalType(LogicalTypeId::FLOAT);
        case parser::DataType::DOUBLE:
            return LogicalType(LogicalTypeId::DOUBLE);
        case parser::DataType::VARCHAR:
        case parser::DataType::TEXT:
            return LogicalType(LogicalTypeId::VARCHAR);
        case parser::DataType::BOOLEAN:
            return LogicalType(LogicalTypeId::BOOLEAN);
        default:
            return LogicalType(LogicalTypeId::SQL_NULL);
    }
}

} // namespace seeddb
