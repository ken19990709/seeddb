#include "executor/executor.h"

#include "common/value.h"
#include "executor/function.h"
#include "storage/schema.h"
#include "storage/storage_manager.h"
#include "storage/table_iterator.h"
#include "storage/tid.h"

#include <algorithm>

namespace seeddb {

// =============================================================================
// Constructor
// =============================================================================

Executor::Executor(Catalog& catalog) : catalog_(catalog) {}

Executor::Executor(Catalog& catalog, StorageManager* storage_mgr)
    : catalog_(catalog), storage_mgr_(storage_mgr) {}

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

    // Persist to disk if storage is available
    if (storage_mgr_) {
        const Table* table = catalog_.getTable(table_name);
        if (!table) {
            return ExecutionResult::error(
                ErrorCode::INTERNAL_ERROR,
                "Internal error: table not found after creation"
            );
        }
        if (!storage_mgr_->onCreateTable(table_name, table->schema())) {
            // Disk creation failed - rollback catalog change
            catalog_.dropTable(table_name);
            return ExecutionResult::error(
                ErrorCode::IO_ERROR,
                "Failed to persist table '" + table_name + "' to disk"
            );
        }
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
    if (storage_mgr_) {
        if (!storage_mgr_->onDropTable(table_name)) {
            return ExecutionResult::error(
                ErrorCode::IO_ERROR,
                "Failed to drop table '" + table_name + "' from disk"
            );
        }
    }
    catalog_.dropTable(table_name);

    return ExecutionResult::empty();
}

// =============================================================================
// DML Execution
// =============================================================================

ExecutionResult Executor::execute(const parser::InsertStmt& stmt) {
    const std::string& table_name = stmt.tableName();

    // Check if table exists
    if (!catalog_.hasTable(table_name)) {
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + table_name + "' not found"
        );
    }

    const Table* table = catalog_.getTable(table_name);
    if (!table) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "Internal error: table lookup failed"
        );
    }
    const Schema& schema = table->schema();

    // Check column count
    const auto& values = stmt.values();
    if (values.size() != schema.columnCount()) {
        return ExecutionResult::error(
            ErrorCode::CONSTRAINT_VIOLATION,
            "Column count mismatch: expected " + std::to_string(schema.columnCount()) +
            ", got " + std::to_string(values.size())
        );
    }

    // Convert AST values to Value objects
    std::vector<Value> row_values;
    row_values.reserve(values.size());

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& expr = values[i];

        // We only support LiteralExpr for INSERT values
        if (expr->type() != parser::NodeType::EXPR_LITERAL) {
            return ExecutionResult::error(
                ErrorCode::NOT_IMPLEMENTED,
                "Only literal values are supported in INSERT"
            );
        }

        const auto* lit = static_cast<const parser::LiteralExpr*>(expr.get());
        const ColumnSchema& col_schema = schema.column(i);

        // Convert literal to Value based on type
        if (lit->isNull()) {
            row_values.push_back(Value::null());
        } else if (lit->isInt()) {
            // Map integer literals to the column's expected type
            int64_t int_val = lit->asInt();
            switch (col_schema.type().id()) {
                case LogicalTypeId::INTEGER:
                    row_values.push_back(Value::integer(static_cast<int32_t>(int_val)));
                    break;
                case LogicalTypeId::BIGINT:
                    row_values.push_back(Value::bigint(int_val));
                    break;
                default:
                    // Default to integer for other types
                    row_values.push_back(Value::integer(static_cast<int32_t>(int_val)));
                    break;
            }
        } else if (lit->isFloat()) {
            // Map float literals to the column's expected type
            double float_val = lit->asFloat();
            switch (col_schema.type().id()) {
                case LogicalTypeId::FLOAT:
                    row_values.push_back(Value::Float(static_cast<float>(float_val)));
                    break;
                case LogicalTypeId::DOUBLE:
                default:
                    row_values.push_back(Value::Double(float_val));
                    break;
            }
        } else if (lit->isString()) {
            row_values.push_back(Value::varchar(lit->asString()));
        } else if (lit->isBool()) {
            row_values.push_back(Value::boolean(lit->asBool()));
        } else {
            return ExecutionResult::error(
                ErrorCode::INTERNAL_ERROR,
                "Unknown literal type in INSERT values"
            );
        }
    }

    // Create a row from the values
    Row row(std::move(row_values));

    // Validate row against schema
    if (!schema.validateRow(row)) {
        return ExecutionResult::error(
            ErrorCode::CONSTRAINT_VIOLATION,
            "Row violates schema constraints"
        );
    }

    if (!storage_mgr_) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "No storage manager available"
        );
    }
    if (!storage_mgr_->insertRow(table_name, row, schema)) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "Failed to insert row"
        );
    }

    return ExecutionResult::empty(1);
}

ExecutionResult Executor::execute(const parser::UpdateStmt& stmt) {
    const std::string& table_name = stmt.tableName();

    // Check if table exists
    if (!catalog_.hasTable(table_name)) {
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + table_name + "' not found"
        );
    }

    if (!storage_mgr_) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "No storage manager available"
        );
    }

    const Table* table = catalog_.getTable(table_name);
    if (!table) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "Internal error: table lookup failed"
        );
    }
    const Schema& schema = table->schema();
    const parser::Expr* where_clause = stmt.whereClause();

    // Two-pass to avoid iterator invalidation from page migration
    std::vector<std::pair<TID, Row>> updates;
    auto iter = storage_mgr_->createIterator(table_name);
    if (!iter) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "Failed to create iterator for table '" + table_name + "'"
        );
    }

    while (iter->next()) {
        const Row& old_row = iter->currentRow();
        if (!where_clause || evaluateWhereClause(where_clause, old_row, schema)) {
            // Build new row with updated values
            std::vector<Value> new_values;
            new_values.reserve(schema.columnCount());

            // Copy old values
            for (size_t i = 0; i < schema.columnCount(); ++i) {
                new_values.push_back(old_row.get(i));
            }

            // Apply assignments
            const auto& assignments = stmt.assignments();
            for (const auto& [col_name, expr] : assignments) {
                auto col_idx_opt = schema.columnIndex(col_name);
                if (!col_idx_opt.has_value()) {
                    return ExecutionResult::error(
                        ErrorCode::COLUMN_NOT_FOUND,
                        "Column '" + col_name + "' not found in table '" + table_name + "'"
                    );
                }
                size_t col_idx = col_idx_opt.value();
                Value new_value = evaluateExpr(expr.get(), old_row, schema);
                new_values[col_idx] = new_value;
            }

            // Validate new row
            Row new_row(std::move(new_values));
            if (!schema.validateRow(new_row)) {
                return ExecutionResult::error(
                    ErrorCode::CONSTRAINT_VIOLATION,
                    "Updated row violates schema constraints"
                );
            }

            updates.emplace_back(iter->currentTID(), std::move(new_row));
        }
    }

    // Apply collected updates
    // NOTE: Without WAL, partial failures cannot be rolled back. Track applied count for diagnostics.
    size_t applied_count = 0;
    for (auto& [tid, new_row] : updates) {
        if (!storage_mgr_->updateRow(tid, new_row, schema)) {
            // Partial failure: some rows were updated, others were not.
            // True atomicity requires WAL/undo log (future enhancement).
            return ExecutionResult::error(
                ErrorCode::INTERNAL_ERROR,
                "Failed to update row (" + std::to_string(applied_count) + " of " +
                std::to_string(updates.size()) + " rows updated before failure)"
            );
        }
        ++applied_count;
    }

    return ExecutionResult::empty();
}

ExecutionResult Executor::execute(const parser::DeleteStmt& stmt) {
    const std::string& table_name = stmt.tableName();

    if (!catalog_.hasTable(table_name)) {
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + table_name + "' not found"
        );
    }

    if (!storage_mgr_) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "No storage manager available"
        );
    }

    const Table* table = catalog_.getTable(table_name);
    if (!table) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "Internal error: table lookup failed"
        );
    }
    const Schema& schema = table->schema();
    const parser::Expr* where_clause = stmt.whereClause();

    // Two-pass to avoid iterator invalidation
    std::vector<TID> tids_to_delete;
    auto iter = storage_mgr_->createIterator(table_name);
    if (!iter) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "Failed to create iterator for table '" + table_name + "'"
        );
    }

    while (iter->next()) {
        if (!where_clause || evaluateWhereClause(where_clause, iter->currentRow(), schema)) {
            tids_to_delete.push_back(iter->currentTID());
        }
    }

    // Apply collected deletes
    // NOTE: Without WAL, partial failures cannot be rolled back. Track applied count for diagnostics.
    size_t applied_count = 0;
    for (const TID& tid : tids_to_delete) {
        if (!storage_mgr_->deleteRow(tid)) {
            // Partial failure: some rows were deleted, others were not.
            // True atomicity requires WAL/undo log (future enhancement).
            return ExecutionResult::error(
                ErrorCode::INTERNAL_ERROR,
                "Failed to delete row (" + std::to_string(applied_count) + " of " +
                std::to_string(tids_to_delete.size()) + " rows deleted before failure)"
            );
        }
        ++applied_count;
    }

    return ExecutionResult::empty();
}

ExecutionResult Executor::execute(const parser::SelectStmt& stmt) {
    // Prepare the SELECT query
    if (!prepareSelect(stmt)) {
        if (!stmt.fromTable()) {
            return ExecutionResult::error(
                ErrorCode::NOT_IMPLEMENTED,
                "SELECT without FROM not implemented"
            );
        }
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + stmt.fromTable()->name() + "' not found"
        );
    }

    // Return first row if available
    if (hasNext()) {
        return next();
    }

    return ExecutionResult::empty();
}

// =============================================================================
// SELECT Execution - Iterator Interface
// =============================================================================

bool Executor::prepareSelect(const parser::SelectStmt& stmt) {
    // Reset any previous query state
    resetQuery();

    // Handle JOIN queries
    if (stmt.hasJoins()) {
        return processJoins(stmt);
    }

    // Original single-table logic
    // Get table name from FROM clause
    const parser::TableRef* table_ref = stmt.fromTable();
    if (!table_ref) {
        return false;
    }

    const std::string& table_name = table_ref->name();

    // Check if table exists
    if (!catalog_.hasTable(table_name)) {
        return false;
    }

    const Table* table_ptr = catalog_.getTable(table_name);
    if (!table_ptr) {
        return false;
    }
    const Schema& schema = table_ptr->schema();

    // Check if this is an aggregate query
    if (hasAggregates(stmt) || stmt.hasGroupBy()) {
        if (!processAggregateQuery(stmt, table_name, schema)) {
            return false;
        }

        // Build alias map and result schema for aggregate results
        std::unordered_map<std::string, std::string> alias_map;
        Schema result_schema = computeAggregateResultSchema(stmt, schema, alias_map);

        // Apply ORDER BY if specified (on aggregate results)
        if (stmt.hasOrderBy()) {
            sortResultRows(stmt, alias_map, result_schema);
        }

        // Apply LIMIT and OFFSET
        applyLimitOffset(stmt);

        return true;
    }

    // Non-aggregate query - original logic
    // Build alias map for column names
    std::unordered_map<std::string, std::string> alias_map;

    // Build result schema for projected columns
    std::vector<ColumnSchema> result_columns;
    if (stmt.isSelectAll()) {
        // SELECT * - use all columns from table
        for (size_t i = 0; i < schema.columnCount(); ++i) {
            result_columns.push_back(schema.column(i));
        }
    } else {
        // Build result schema from select list
        for (const auto& item : stmt.selectItems()) {
            if (item.expr->type() == parser::NodeType::EXPR_COLUMN_REF) {
                const auto* col_ref = static_cast<const parser::ColumnRef*>(item.expr.get());
                const std::string& col_name = col_ref->column();

                // Find column in schema
                auto col_idx = schema.columnIndex(col_name);
                if (col_idx.has_value()) {
                    result_columns.push_back(schema.column(col_idx.value()));
                    // Store alias if present
                    if (item.hasAlias()) {
                        alias_map[col_name] = item.alias;
                    }
                }
            }
        }
    }
    Schema result_schema(std::move(result_columns));

    // Find matching rows and project them
    const parser::Expr* where_clause = stmt.whereClause();

    if (!storage_mgr_) {
        return false;
    }
    auto iter = storage_mgr_->createIterator(table_name);
    if (!iter) {
        return false;
    }
    while (iter->next()) {
        const Row& row = iter->currentRow();

        // Apply WHERE clause filter
        if (where_clause && !evaluateWhereClause(where_clause, row, schema)) {
            continue;
        }

        // Project row
        Row projected = projectRow(row, schema, stmt, alias_map);
        result_rows_.push_back(std::move(projected));
    }

    // Apply DISTINCT if specified
    applyDistinct(stmt);

    // Apply ORDER BY if specified
    if (stmt.hasOrderBy()) {
        sortResultRows(stmt, alias_map, result_schema);
    }

    // Apply LIMIT and OFFSET
    applyLimitOffset(stmt);

    return true;
}

bool Executor::hasNext() const {
    return current_row_index_ < result_rows_.size();
}

ExecutionResult Executor::next() {
    if (!hasNext()) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "No more rows available"
        );
    }

    // Get the result row
    Row row = std::move(result_rows_[current_row_index_]);

    // Advance to next row
    current_row_index_++;

    return ExecutionResult::ok(std::move(row));
}

void Executor::resetQuery() {
    result_rows_.clear();
    current_row_index_ = 0;
    
    // Reset JOIN state
    table_schemas_.clear();
    table_aliases_.clear();
    joined_rows_.clear();
    joined_schema_ = Schema(std::vector<ColumnSchema>{});
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

bool Executor::evaluateWhereClause(const parser::Expr* expr, const Row& row, const Schema& schema) const {
    if (!expr) {
        return true;  // No WHERE clause means all rows match
    }

    Value result = evaluateExpr(expr, row, schema);

    // NULL in WHERE clause is treated as false
    if (result.isNull()) {
        return false;
    }

    // Boolean result
    if (result.typeId() == LogicalTypeId::BOOLEAN) {
        return result.asBool();
    }

    // Non-boolean values are treated as true (though this shouldn't happen in valid SQL)
    return true;
}

Value Executor::evaluateExpr(const parser::Expr* expr, const Row& row, const Schema& schema) const {
    if (!expr) {
        return Value::null();
    }

    switch (expr->type()) {
        case parser::NodeType::EXPR_LITERAL: {
            const auto* lit = static_cast<const parser::LiteralExpr*>(expr);

            if (lit->isNull()) {
                return Value::null();
            } else if (lit->isInt()) {
                return Value::bigint(lit->asInt());
            } else if (lit->isFloat()) {
                return Value::Double(lit->asFloat());
            } else if (lit->isString()) {
                return Value::varchar(lit->asString());
            } else if (lit->isBool()) {
                return Value::boolean(lit->asBool());
            }
            return Value::null();
        }

        case parser::NodeType::EXPR_COLUMN_REF: {
            const auto* col_ref = static_cast<const parser::ColumnRef*>(expr);
            const std::string& col_name = col_ref->column();

            auto col_idx = schema.columnIndex(col_name);
            if (col_idx.has_value()) {
                return row.get(col_idx.value());
            }
            return Value::null();
        }

        case parser::NodeType::EXPR_BINARY: {
            const auto* binary = static_cast<const parser::BinaryExpr*>(expr);
            const std::string& op = binary->op();

            Value left = evaluateExpr(binary->left(), row, schema);
            Value right = evaluateExpr(binary->right(), row, schema);

            // Handle logical operators
            if (op == "AND") {
                if (left.isNull() || right.isNull()) {
                    return Value::null();
                }
                return Value::boolean(left.asBool() && right.asBool());
            }
            if (op == "OR") {
                if (left.isNull() && right.isNull()) {
                    return Value::null();
                }
                if (left.isNull()) {
                    return Value::boolean(right.asBool());
                }
                if (right.isNull()) {
                    return Value::boolean(left.asBool());
                }
                return Value::boolean(left.asBool() || right.asBool());
            }

            // NULL comparison handling for comparison operators
            if (left.isNull() || right.isNull()) {
                return Value::null();
            }

            // Helper lambda to check if a type is numeric
            auto is_numeric_type = [](LogicalTypeId id) {
                return id == LogicalTypeId::INTEGER ||
                       id == LogicalTypeId::BIGINT ||
                       id == LogicalTypeId::FLOAT ||
                       id == LogicalTypeId::DOUBLE;
            };

            // Comparison operators with type coercion for numeric types
            if (op == "=") {
                // If both are numeric, compare as doubles
                if (is_numeric_type(left.typeId()) && is_numeric_type(right.typeId())) {
                    return Value::boolean(to_double(left) == to_double(right));
                }
                return Value::boolean(left.equals(right));
            }
            if (op == "<>" || op == "!=") {
                // If both are numeric, compare as doubles
                if (is_numeric_type(left.typeId()) && is_numeric_type(right.typeId())) {
                    return Value::boolean(to_double(left) != to_double(right));
                }
                return Value::boolean(!left.equals(right));
            }
            if (op == "<") {
                // If both are numeric, compare as doubles
                if (is_numeric_type(left.typeId()) && is_numeric_type(right.typeId())) {
                    return Value::boolean(to_double(left) < to_double(right));
                }
                return Value::boolean(left.lessThan(right));
            }
            if (op == ">") {
                // If both are numeric, compare as doubles
                if (is_numeric_type(left.typeId()) && is_numeric_type(right.typeId())) {
                    return Value::boolean(to_double(left) > to_double(right));
                }
                return Value::boolean(right.lessThan(left));
            }
            if (op == "<=") {
                // If both are numeric, compare as doubles
                if (is_numeric_type(left.typeId()) && is_numeric_type(right.typeId())) {
                    double l = to_double(left), r = to_double(right);
                    return Value::boolean(l <= r);
                }
                return Value::boolean(left.lessThan(right) || left.equals(right));
            }
            if (op == ">=") {
                // If both are numeric, compare as doubles
                if (is_numeric_type(left.typeId()) && is_numeric_type(right.typeId())) {
                    double l = to_double(left), r = to_double(right);
                    return Value::boolean(l >= r);
                }
                return Value::boolean(right.lessThan(left) || left.equals(right));
            }

            // Arithmetic operators (for numeric types)
            // Simplified: only handle INTEGER and DOUBLE
            if (left.typeId() == LogicalTypeId::INTEGER && right.typeId() == LogicalTypeId::INTEGER) {
                int32_t l = left.asInt32();
                int32_t r = right.asInt32();
                if (op == "+") return Value::integer(l + r);
                if (op == "-") return Value::integer(l - r);
                if (op == "*") return Value::integer(l * r);
                if (op == "/") {
                    if (r == 0) return Value::null();
                    return Value::integer(l / r);
                }
            }

            // Handle as doubles for mixed arithmetic
            double l = 0.0, r = 0.0;
            if (left.typeId() == LogicalTypeId::INTEGER) l = left.asInt32();
            else if (left.typeId() == LogicalTypeId::BIGINT) l = static_cast<double>(left.asInt64());
            else if (left.typeId() == LogicalTypeId::FLOAT) l = left.asFloat();
            else if (left.typeId() == LogicalTypeId::DOUBLE) l = left.asDouble();

            if (right.typeId() == LogicalTypeId::INTEGER) r = right.asInt32();
            else if (right.typeId() == LogicalTypeId::BIGINT) r = static_cast<double>(right.asInt64());
            else if (right.typeId() == LogicalTypeId::FLOAT) r = right.asFloat();
            else if (right.typeId() == LogicalTypeId::DOUBLE) r = right.asDouble();

            if (op == "+") return Value::Double(l + r);
            if (op == "-") return Value::Double(l - r);
            if (op == "*") return Value::Double(l * r);
            if (op == "/") {
                if (r == 0.0) return Value::null();
                return Value::Double(l / r);
            }

            return Value::null();
        }

        case parser::NodeType::EXPR_UNARY: {
            const auto* unary = static_cast<const parser::UnaryExpr*>(expr);
            const std::string& op = unary->op();

            Value operand = evaluateExpr(unary->operand(), row, schema);

            if (op == "NOT") {
                if (operand.isNull()) {
                    return Value::null();
                }
                return Value::boolean(!operand.asBool());
            }
            if (op == "-") {
                if (operand.isNull()) {
                    return Value::null();
                }
                if (operand.typeId() == LogicalTypeId::INTEGER) {
                    return Value::integer(-operand.asInt32());
                }
                if (operand.typeId() == LogicalTypeId::BIGINT) {
                    return Value::bigint(-operand.asInt64());
                }
                if (operand.typeId() == LogicalTypeId::FLOAT) {
                    return Value::Float(-operand.asFloat());
                }
                if (operand.typeId() == LogicalTypeId::DOUBLE) {
                    return Value::Double(-operand.asDouble());
                }
            }
            if (op == "+") {
                return operand;  // Unary plus is identity
            }

            return Value::null();
        }

        case parser::NodeType::EXPR_IS_NULL: {
            const auto* is_null = static_cast<const parser::IsNullExpr*>(expr);
            Value val = evaluateExpr(is_null->expr(), row, schema);
            bool is_null_result = val.isNull();
            return Value::boolean(is_null->isNegated() ? !is_null_result : is_null_result);
        }

        case parser::NodeType::EXPR_CASE: {
            const auto* case_expr = static_cast<const parser::CaseExpr*>(expr);
            // Evaluate each WHEN clause in order
            for (const auto& when_clause : case_expr->whenClauses()) {
                Value cond = evaluateExpr(when_clause.when_expr.get(), row, schema);
                // NULL condition is treated as false
                if (!cond.isNull() && cond.asBool()) {
                    return evaluateExpr(when_clause.then_expr.get(), row, schema);
                }
            }
            // No WHEN clause matched - return ELSE or NULL
            if (case_expr->hasElse()) {
                return evaluateExpr(case_expr->elseExpr(), row, schema);
            }
            return Value::null();
        }

        case parser::NodeType::EXPR_IN: {
            const auto* in_expr = static_cast<const parser::InExpr*>(expr);
            Value left = evaluateExpr(in_expr->expr(), row, schema);

            // NULL left operand with non-empty list returns NULL (not false)
            if (left.isNull()) {
                return Value::null();
            }

            // Check membership in value list
            for (const auto& val_expr : in_expr->values()) {
                Value val = evaluateExpr(val_expr.get(), row, schema);
                if (val.isNull()) {
                    // NULL in list means result could be NULL (but for simplicity, we continue)
                    continue;
                }
                if (left.equals(val)) {
                    return Value::boolean(!in_expr->isNegated());
                }
            }
            // No match found
            return Value::boolean(in_expr->isNegated());
        }

        case parser::NodeType::EXPR_BETWEEN: {
            const auto* between_expr = static_cast<const parser::BetweenExpr*>(expr);
            Value val = evaluateExpr(between_expr->expr(), row, schema);
            Value low = evaluateExpr(between_expr->low(), row, schema);
            Value high = evaluateExpr(between_expr->high(), row, schema);

            // If any operand is NULL, result is NULL
            if (val.isNull() || low.isNull() || high.isNull()) {
                return Value::null();
            }

            // Check if val is in range [low, high]
            bool in_range = (!val.lessThan(low) && !high.lessThan(val));
            return Value::boolean(between_expr->isNegated() ? !in_range : in_range);
        }

        case parser::NodeType::EXPR_LIKE: {
            const auto* like_expr = static_cast<const parser::LikeExpr*>(expr);
            Value str = evaluateExpr(like_expr->str(), row, schema);
            Value pattern = evaluateExpr(like_expr->pattern(), row, schema);

            // If any operand is NULL, result is NULL
            if (str.isNull() || pattern.isNull()) {
                return Value::null();
            }

            // Perform pattern matching
            bool matches = matchLikePattern(str.asString(), pattern.asString());
            return Value::boolean(like_expr->isNegated() ? !matches : matches);
        }

        case parser::NodeType::EXPR_COALESCE: {
            const auto* coalesce_expr = static_cast<const parser::CoalesceExpr*>(expr);
            // Return first non-NULL argument
            for (const auto& arg : coalesce_expr->args()) {
                Value val = evaluateExpr(arg.get(), row, schema);
                if (!val.isNull()) {
                    return val;
                }
            }
            // All arguments were NULL
            return Value::null();
        }

        case parser::NodeType::EXPR_NULLIF: {
            const auto* nullif_expr = static_cast<const parser::NullifExpr*>(expr);
            Value expr1 = evaluateExpr(nullif_expr->expr1(), row, schema);
            Value expr2 = evaluateExpr(nullif_expr->expr2(), row, schema);

            // If equal, return NULL; otherwise return expr1
            if (!expr1.isNull() && !expr2.isNull() && expr1.equals(expr2)) {
                return Value::null();
            }
            return expr1;
        }

        case parser::NodeType::EXPR_FUNCTION_CALL: {
            const auto* func_expr = static_cast<const parser::FunctionCallExpr*>(expr);
            const std::string& func_name = func_expr->functionName();
            
            // Lookup function in registry
            const FunctionInfo* func_info = FunctionRegistry::instance().lookup(func_name);
            if (!func_info) {
                // Unknown function returns NULL
                return Value::null();
            }
            
            // Validate argument count
            size_t arg_count = func_expr->argCount();
            if (arg_count < func_info->min_args || arg_count > func_info->max_args) {
                // Wrong argument count returns NULL
                return Value::null();
            }
            
            // Evaluate arguments
            std::vector<Value> args;
            args.reserve(arg_count);
            for (const auto& arg : func_expr->args()) {
                args.push_back(evaluateExpr(arg.get(), row, schema));
            }
            
            // Call function implementation
            return func_info->impl(args);
        }

        default:
            return Value::null();
    }
}

Row Executor::projectRow(const Row& row, const Schema& schema, const parser::SelectStmt& stmt,
                        std::unordered_map<std::string, std::string>& alias_map) const {
    (void)alias_map;  // Reserved for future alias resolution during projection
    if (stmt.isSelectAll()) {
        // SELECT * - return all columns
        return Row(row);
    }

    // Project specific columns
    std::vector<Value> projected_values;
    for (const auto& item : stmt.selectItems()) {
        Value val = evaluateExpr(item.expr.get(), row, schema);
        projected_values.push_back(std::move(val));
    }
    return Row(std::move(projected_values));
}

bool Executor::rowsEqual(const Row& a, const Row& b) const {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const Value& va = a.get(i);
        const Value& vb = b.get(i);
        
        // NULLs are equal
        if (va.isNull() && vb.isNull()) {
            continue;
        }
        // One NULL, one not NULL - not equal
        if (va.isNull() || vb.isNull()) {
            return false;
        }
        // Compare values
        if (!va.equals(vb)) {
            return false;
        }
    }
    return true;
}

int Executor::compareValues(const Value& a, const Value& b) const {
    // NULL handling - NULLs sort last
    if (a.isNull() && b.isNull()) {
        return 0;
    }
    if (a.isNull()) {
        return 1;  // NULL > non-NULL (NULLs last)
    }
    if (b.isNull()) {
        return -1; // non-NULL < NULL (NULLs last)
    }

    // Compare based on type
    if (a.lessThan(b)) {
        return -1;
    }
    if (b.lessThan(a)) {
        return 1;
    }
    return 0;  // Equal
}

void Executor::sortResultRows(const parser::SelectStmt& stmt,
                               const std::unordered_map<std::string, std::string>& alias_map,
                               const Schema& result_schema) {
    const auto& order_by = stmt.orderBy();
    
    std::sort(result_rows_.begin(), result_rows_.end(),
        [&](const Row& a, const Row& b) {
            for (const auto& order_item : order_by) {
                // Get the column name (could be alias or actual name)
                std::string col_name;
                if (order_item.expr->type() == parser::NodeType::EXPR_COLUMN_REF) {
                    col_name = static_cast<const parser::ColumnRef*>(order_item.expr.get())->column();
                }

                // Check if it's an alias
                std::string actual_col_name = col_name;
                for (const auto& [orig, alias] : alias_map) {
                    if (alias == col_name) {
                        actual_col_name = orig;
                        break;
                    }
                }

                // Find column index in result schema
                size_t col_idx = 0;
                bool found = false;
                for (size_t i = 0; i < result_schema.columnCount(); ++i) {
                    if (result_schema.column(i).name() == actual_col_name) {
                        col_idx = i;
                        found = true;
                        break;
                    }
                }
                
                // Skip if column not found in result schema
                if (!found) {
                    continue;
                }

                const Value& va = a.get(col_idx);
                const Value& vb = b.get(col_idx);
                
                int cmp = compareValues(va, vb);
                if (cmp != 0) {
                    // Apply direction
                    return (order_item.direction == parser::SortDirection::DESC) ? (cmp > 0) : (cmp < 0);
                }
            }
            return false;  // All columns equal, maintain original order
        });
}

// =============================================================================
// JOIN Implementation
// =============================================================================

bool Executor::processJoins(const parser::SelectStmt& stmt) {
    // Collect all tables involved in the query
    std::vector<const parser::TableRef*> all_tables;
    
    // Add base table from FROM clause
    if (stmt.fromTable()) {
        all_tables.push_back(stmt.fromTable());
    }
    
    // Add joined tables
    for (const auto& join : stmt.joins()) {
        all_tables.push_back(join->table());
    }
    
    if (all_tables.empty()) {
        return false;
    }
    
    // Validate all tables exist and build schema map
    table_schemas_.clear();
    table_aliases_.clear();

    for (const auto* table_ref : all_tables) {
        const std::string& table_name = table_ref->name();

        // Check if table exists
        if (!catalog_.hasTable(table_name)) {
            return false;
        }

        Table* table = catalog_.getTable(table_name);
        if (!table) {
            return false;
        }

        table_schemas_[table_name] = &table->schema();

        // Store alias mapping
        if (table_ref->hasAlias()) {
            table_aliases_[table_ref->alias()] = table_name;
        } else {
            table_aliases_[table_name] = table_name;  // Self-mapping for unaliased tables
        }
    }

    // Build combined schema for all tables
    joined_schema_ = buildJoinedSchema(stmt, table_schemas_);

    if (!storage_mgr_) return false;

    // Start with the first table's rows (via iterator)
    std::vector<Row> current_rows;
    auto first_iter = storage_mgr_->createIterator(all_tables[0]->name());
    if (!first_iter) return false;
    while (first_iter->next()) {
        current_rows.push_back(Row(first_iter->currentRow()));
    }
    
    // Process each join in sequence
    const Schema* left_schema = table_schemas_[all_tables[0]->name()];
    
    for (size_t join_idx = 0; join_idx < stmt.joins().size(); ++join_idx) {
        const auto& join = stmt.joins()[join_idx];
        const parser::TableRef* right_table_ref = join->table();
        const std::string& right_table_name = right_table_ref->name();
        const Schema* right_schema = table_schemas_[right_table_name];

        // Get right table rows via iterator
        auto right_iter = storage_mgr_->createIterator(right_table_name);
        if (!right_iter) return false;
        std::vector<Row> right_rows;
        while (right_iter->next()) {
            right_rows.push_back(Row(right_iter->currentRow()));
        }
        
        // Perform join based on type
        std::vector<std::pair<Row, Row>> joined_pairs;
        
        switch (join->joinType()) {
            case parser::JoinType::CROSS:
                joined_pairs = crossJoinRows(current_rows, right_rows);
                break;
                
            case parser::JoinType::INNER:
                if (!join->hasCondition()) {
                    // No condition means cross join
                    joined_pairs = crossJoinRows(current_rows, right_rows);
                } else {
                    joined_pairs = innerJoinRows(current_rows, right_rows, 
                                               join->condition(), *left_schema, *right_schema);
                }
                break;
                
            default:
                // Unsupported join type for now
                return false;
        }
        
        // Combine paired rows into single rows
        std::vector<Row> next_rows;
        for (const auto& pair : joined_pairs) {
            // Concatenate left and right rows
            std::vector<Value> combined_values;
            combined_values.reserve(pair.first.size() + pair.second.size());
            
            for (size_t i = 0; i < pair.first.size(); ++i) {
                combined_values.push_back(pair.first.get(i));
            }
            for (size_t i = 0; i < pair.second.size(); ++i) {
                combined_values.push_back(pair.second.get(i));
            }
            
            next_rows.emplace_back(std::move(combined_values));
        }
        
        current_rows = std::move(next_rows);
        left_schema = &joined_schema_;  // Updated schema after join
    }
    
    joined_rows_ = std::move(current_rows);
    
    // Apply WHERE clause if present
    const parser::Expr* where_clause = stmt.whereClause();
    if (where_clause) {
        std::vector<Row> filtered_rows;
        for (const auto& row : joined_rows_) {
            if (evaluateWhereClause(where_clause, row, joined_schema_)) {
                filtered_rows.push_back(row);
            }
        }
        joined_rows_ = std::move(filtered_rows);
    }
    
    // Project rows according to SELECT clause
    std::unordered_map<std::string, std::string> alias_map;
    result_rows_.clear();
    
    for (const auto& row : joined_rows_) {
        Row projected = projectRow(row, joined_schema_, stmt, alias_map);
        result_rows_.push_back(std::move(projected));
    }
    
    // Apply DISTINCT if specified
    applyDistinct(stmt);

    // Apply ORDER BY if specified
    if (stmt.hasOrderBy()) {
        sortResultRows(stmt, alias_map, joined_schema_);
    }

    // Apply LIMIT and OFFSET
    applyLimitOffset(stmt);
    
    return true;
}

// =============================================================================
// JOIN Helper Methods
// =============================================================================

Schema Executor::buildJoinedSchema(const parser::SelectStmt& stmt,
                                  const std::unordered_map<std::string, const Schema*>& schemas) const {
    std::vector<ColumnSchema> columns;
    
    // Add columns from base table
    if (stmt.fromTable()) {
        const std::string& table_name = stmt.fromTable()->name();
        auto it = schemas.find(table_name);
        if (it != schemas.end()) {
            const Schema* schema = it->second;
            for (size_t i = 0; i < schema->columnCount(); ++i) {
                columns.push_back(schema->column(i));
            }
        }
    }
    
    // Add columns from joined tables
    for (const auto& join : stmt.joins()) {
        const std::string& table_name = join->table()->name();
        auto it = schemas.find(table_name);
        if (it != schemas.end()) {
            const Schema* schema = it->second;
            for (size_t i = 0; i < schema->columnCount(); ++i) {
                columns.push_back(schema->column(i));
            }
        }
    }
    
    return Schema(std::move(columns));
}

std::pair<const Schema*, size_t> Executor::resolveColumnInJoinedContext(
    const parser::ColumnRef* col_ref,
    const std::unordered_map<std::string, const Schema*>& schemas,
    const std::unordered_map<std::string, std::string>& table_aliases) const {
    
    const std::string& column_name = col_ref->column();
    
    // If column has table qualifier
    if (col_ref->hasTableQualifier()) {
        const std::string& table_qualifier = col_ref->table();
        
        // Resolve alias to actual table name
        auto alias_it = table_aliases.find(table_qualifier);
        if (alias_it == table_aliases.end()) {
            return {nullptr, 0};
        }
        
        const std::string& actual_table_name = alias_it->second;
        
        // Find schema for the table
        auto schema_it = schemas.find(actual_table_name);
        if (schema_it == schemas.end()) {
            return {nullptr, 0};
        }
        
        const Schema* schema = schema_it->second;
        auto col_idx = schema->columnIndex(column_name);
        if (!col_idx.has_value()) {
            return {nullptr, 0};
        }
        
        return {schema, col_idx.value()};
    } else {
        // No table qualifier - search all schemas
        for (const auto& [table_name, schema] : schemas) {
            auto col_idx = schema->columnIndex(column_name);
            if (col_idx.has_value()) {
                return {schema, col_idx.value()};
            }
        }
    }
    
    return {nullptr, 0};
}

std::vector<std::pair<Row, Row>> Executor::crossJoinRows(
    const std::vector<Row>& left_rows,
    const std::vector<Row>& right_rows) const {
    
    std::vector<std::pair<Row, Row>> result;
    result.reserve(left_rows.size() * right_rows.size());
    
    for (const auto& left_row : left_rows) {
        for (const auto& right_row : right_rows) {
            result.emplace_back(left_row, right_row);
        }
    }
    
    return result;
}

std::vector<std::pair<Row, Row>> Executor::innerJoinRows(
    const std::vector<Row>& left_rows,
    const std::vector<Row>& right_rows,
    const parser::Expr* condition,
    const Schema& left_schema,
    const Schema& right_schema) const {
    
    std::vector<std::pair<Row, Row>> result;
    
    // Create combined schema for evaluation
    std::vector<ColumnSchema> combined_columns;
    for (size_t i = 0; i < left_schema.columnCount(); ++i) {
        combined_columns.push_back(left_schema.column(i));
    }
    for (size_t i = 0; i < right_schema.columnCount(); ++i) {
        combined_columns.push_back(right_schema.column(i));
    }
    Schema combined_schema(std::move(combined_columns));
    
    for (const auto& left_row : left_rows) {
        for (const auto& right_row : right_rows) {
            // Create combined row for condition evaluation
            std::vector<Value> combined_values;
            combined_values.reserve(left_row.size() + right_row.size());
            
            for (size_t i = 0; i < left_row.size(); ++i) {
                combined_values.push_back(left_row.get(i));
            }
            for (size_t i = 0; i < right_row.size(); ++i) {
                combined_values.push_back(right_row.get(i));
            }
            
            Row combined_row(std::move(combined_values));
            
            // Evaluate join condition
            if (evaluateWhereClause(condition, combined_row, combined_schema)) {
                result.emplace_back(left_row, right_row);
            }
        }
    }
    
    return result;
}

// =============================================================================
// Aggregate Helper Methods
// =============================================================================

bool Executor::hasAggregates(const parser::SelectStmt& stmt) const {
    // Check SELECT list
    for (const auto& item : stmt.selectItems()) {
        if (exprHasAggregates(item.expr.get())) {
            return true;
        }
    }
    // Check HAVING clause
    if (stmt.hasHaving() && exprHasAggregates(stmt.havingClause())) {
        return true;
    }
    return false;
}

bool Executor::exprHasAggregates(const parser::Expr* expr) const {
    if (!expr) {
        return false;
    }

    switch (expr->type()) {
        case parser::NodeType::EXPR_AGGREGATE:
            return true;

        case parser::NodeType::EXPR_BINARY: {
            const auto* binary = static_cast<const parser::BinaryExpr*>(expr);
            return exprHasAggregates(binary->left()) || exprHasAggregates(binary->right());
        }

        case parser::NodeType::EXPR_UNARY: {
            const auto* unary = static_cast<const parser::UnaryExpr*>(expr);
            return exprHasAggregates(unary->operand());
        }

        case parser::NodeType::EXPR_IS_NULL: {
            const auto* is_null = static_cast<const parser::IsNullExpr*>(expr);
            return exprHasAggregates(is_null->expr());
        }

        default:
            return false;
    }
}

GroupKey Executor::extractGroupKey(const Row& row, const Schema& schema,
                                   const parser::SelectStmt& stmt) const {
    GroupKey key;
    for (const auto& group_expr : stmt.groupBy()) {
        Value val = evaluateExpr(group_expr.get(), row, schema);
        key.push_back(std::move(val));
    }
    return key;
}

std::unique_ptr<AggregateState> Executor::createAggregateState(const parser::SelectStmt& stmt) const {
    auto state = std::make_unique<AggregateState>();
    auto aggregates = collectAggregates(stmt);
    for (const auto* agg : aggregates) {
        state->addAccumulator(createAccumulator(agg));
    }
    return state;
}

std::unique_ptr<AggregateAccumulator> Executor::createAccumulator(const parser::AggregateExpr* agg) const {
    switch (agg->aggType()) {
        case parser::AggregateType::COUNT:
            if (agg->isDistinct()) {
                return std::make_unique<CountDistinctAccumulator>();
            }
            return std::make_unique<CountAccumulator>(agg->isStar());

        case parser::AggregateType::SUM:
            return std::make_unique<SumAccumulator>();

        case parser::AggregateType::AVG:
            return std::make_unique<AvgAccumulator>();

        case parser::AggregateType::MIN:
            return std::make_unique<MinAccumulator>();

        case parser::AggregateType::MAX:
            return std::make_unique<MaxAccumulator>();

        default:
            return std::make_unique<CountAccumulator>(true);
    }
}

std::vector<const parser::AggregateExpr*> Executor::collectAggregates(const parser::SelectStmt& stmt) const {
    std::vector<const parser::AggregateExpr*> aggregates;

    // Collect from SELECT list
    for (const auto& item : stmt.selectItems()) {
        collectAggregatesFromExpr(item.expr.get(), aggregates);
    }

    // Collect from HAVING clause
    if (stmt.hasHaving()) {
        collectAggregatesFromExpr(stmt.havingClause(), aggregates);
    }

    return aggregates;
}

void Executor::collectAggregatesFromExpr(const parser::Expr* expr,
                                         std::vector<const parser::AggregateExpr*>& aggregates) const {
    if (!expr) {
        return;
    }

    switch (expr->type()) {
        case parser::NodeType::EXPR_AGGREGATE: {
            const auto* agg = static_cast<const parser::AggregateExpr*>(expr);
            aggregates.push_back(agg);
            // Also recurse into aggregate argument
            collectAggregatesFromExpr(agg->arg(), aggregates);
            break;
        }

        case parser::NodeType::EXPR_BINARY: {
            const auto* binary = static_cast<const parser::BinaryExpr*>(expr);
            collectAggregatesFromExpr(binary->left(), aggregates);
            collectAggregatesFromExpr(binary->right(), aggregates);
            break;
        }

        case parser::NodeType::EXPR_UNARY: {
            const auto* unary = static_cast<const parser::UnaryExpr*>(expr);
            collectAggregatesFromExpr(unary->operand(), aggregates);
            break;
        }

        case parser::NodeType::EXPR_IS_NULL: {
            const auto* is_null = static_cast<const parser::IsNullExpr*>(expr);
            collectAggregatesFromExpr(is_null->expr(), aggregates);
            break;
        }

        default:
            break;
    }
}

bool Executor::processAggregateQuery(const parser::SelectStmt& stmt,
                                      const std::string& table_name,
                                      const Schema& schema) {
    auto validation = validateAggregateQuery(stmt, schema);
    if (validation.status() == ExecutionResult::Status::ERROR) {
        result_rows_.clear();
        return false;
    }

    auto aggregates = collectAggregates(stmt);
    auto template_state = createAggregateState(stmt);

    std::unordered_map<GroupKey, std::unique_ptr<AggregateState>, GroupKeyHash, GroupKeyEqual> groups;
    bool has_group_by = stmt.hasGroupBy();
    const parser::Expr* where_clause = stmt.whereClause();

    auto iter = storage_mgr_->createIterator(table_name);
    if (!iter) {
        result_rows_.clear();
        return false;
    }

    while (iter->next()) {
        const Row& row = iter->currentRow();

        // Apply WHERE clause filter
        if (where_clause && !evaluateWhereClause(where_clause, row, schema)) {
            continue;
        }

        // Extract group key (empty for no GROUP BY)
        GroupKey key;
        if (has_group_by) {
            key = extractGroupKey(row, schema, stmt);
        }

        // Get or create group state
        auto it = groups.find(key);
        if (it == groups.end()) {
            auto state = template_state->clone();
            it = groups.emplace(key, std::move(state)).first;
        }

        // Accumulate values for each aggregate
        for (size_t agg_idx = 0; agg_idx < aggregates.size(); ++agg_idx) {
            const auto* agg = aggregates[agg_idx];

            Value val;
            if (agg->isStar()) {
                // COUNT(*) - just pass a non-null value
                val = Value::integer(1);
            } else if (agg->arg()) {
                val = evaluateExpr(agg->arg(), row, schema);
            } else {
                val = Value::null();
            }
            it->second->accumulate(agg_idx, val);
        }
    }

    // Handle empty result (no rows matched WHERE)
    if (groups.empty() && !has_group_by) {
        // With no GROUP BY, still produce one row with aggregate of empty set
        groups.emplace(GroupKey{}, template_state->clone());
    }

    // Finalize groups and build result rows
    for (auto& [group_key, state] : groups) {
        // Finalize aggregate values
        auto agg_values = state->finalize();

        // Apply HAVING filter if present
        if (stmt.hasHaving()) {
            Value having_result = evaluateExprWithAggregates(
                stmt.havingClause(), group_key, agg_values, aggregates, stmt, schema);
            if (having_result.isNull() || !having_result.asBool()) {
                continue;  // Skip this group
            }
        }

        // Build result row from SELECT list
        std::vector<Value> row_values;
        for (const auto& item : stmt.selectItems()) {
            Value val = evaluateExprWithAggregates(
                item.expr.get(), group_key, agg_values, aggregates, stmt, schema);
            row_values.push_back(std::move(val));
        }
        result_rows_.push_back(Row(std::move(row_values)));
    }
    return true;
}

Value Executor::evaluateExprWithAggregates(const parser::Expr* expr,
                                           const GroupKey& group_key,
                                           const std::vector<Value>& agg_values,
                                           const std::vector<const parser::AggregateExpr*>& aggregates,
                                           const parser::SelectStmt& stmt,
                                           const Schema& schema) const {
    if (!expr) {
        return Value::null();
    }

    switch (expr->type()) {
        case parser::NodeType::EXPR_AGGREGATE: {
            // Find this aggregate in our list and return its finalized value
            const auto* agg = static_cast<const parser::AggregateExpr*>(expr);
            for (size_t i = 0; i < aggregates.size(); ++i) {
                if (aggregates[i] == agg) {
                    return agg_values[i];
                }
            }
            return Value::null();
        }

        case parser::NodeType::EXPR_COLUMN_REF: {
            // Column reference - must be in GROUP BY
            const auto* col_ref = static_cast<const parser::ColumnRef*>(expr);
            const std::string& col_name = col_ref->column();

            // Find in GROUP BY columns
            for (size_t i = 0; i < stmt.groupBy().size(); ++i) {
                const auto* group_expr = stmt.groupBy()[i].get();
                if (group_expr->type() == parser::NodeType::EXPR_COLUMN_REF) {
                    const auto* group_col = static_cast<const parser::ColumnRef*>(group_expr);
                    if (group_col->column() == col_name) {
                        if (i < group_key.size()) {
                            return group_key[i];
                        }
                    }
                }
            }
            // Not in GROUP BY - this should be caught by validation
            return Value::null();
        }

        case parser::NodeType::EXPR_LITERAL: {
            const auto* lit = static_cast<const parser::LiteralExpr*>(expr);
            if (lit->isNull()) return Value::null();
            if (lit->isInt()) return Value::bigint(lit->asInt());
            if (lit->isFloat()) return Value::Double(lit->asFloat());
            if (lit->isString()) return Value::varchar(lit->asString());
            if (lit->isBool()) return Value::boolean(lit->asBool());
            return Value::null();
        }

        case parser::NodeType::EXPR_BINARY: {
            const auto* binary = static_cast<const parser::BinaryExpr*>(expr);
            const std::string& op = binary->op();

            Value left = evaluateExprWithAggregates(binary->left(), group_key, agg_values, aggregates, stmt, schema);
            Value right = evaluateExprWithAggregates(binary->right(), group_key, agg_values, aggregates, stmt, schema);

            // Reuse logic from evaluateExpr for operators
            // Handle logical operators
            if (op == "AND") {
                if (left.isNull() || right.isNull()) return Value::null();
                return Value::boolean(left.asBool() && right.asBool());
            }
            if (op == "OR") {
                if (left.isNull() && right.isNull()) return Value::null();
                if (left.isNull()) return Value::boolean(right.asBool());
                if (right.isNull()) return Value::boolean(left.asBool());
                return Value::boolean(left.asBool() || right.asBool());
            }

            if (left.isNull() || right.isNull()) return Value::null();

            // Comparison operators
            if (op == "=") return Value::boolean(left.equals(right));
            if (op == "<>" || op == "!=") return Value::boolean(!left.equals(right));
            if (op == "<") return Value::boolean(left.lessThan(right));
            if (op == ">") return Value::boolean(right.lessThan(left));
            if (op == "<=") return Value::boolean(left.lessThan(right) || left.equals(right));
            if (op == ">=") return Value::boolean(right.lessThan(left) || left.equals(right));

            // Arithmetic operators - simplified
            double l = to_double(left), r = to_double(right);
            if (op == "+") return Value::Double(l + r);
            if (op == "-") return Value::Double(l - r);
            if (op == "*") return Value::Double(l * r);
            if (op == "/") {
                if (r == 0.0) return Value::null();
                return Value::Double(l / r);
            }

            return Value::null();
        }

        case parser::NodeType::EXPR_UNARY: {
            const auto* unary = static_cast<const parser::UnaryExpr*>(expr);
            Value operand = evaluateExprWithAggregates(unary->operand(), group_key, agg_values, aggregates, stmt, schema);

            if (unary->op() == "NOT") {
                if (operand.isNull()) return Value::null();
                return Value::boolean(!operand.asBool());
            }
            if (unary->op() == "-") {
                if (operand.isNull()) return Value::null();
                if (operand.typeId() == LogicalTypeId::DOUBLE) return Value::Double(-operand.asDouble());
                if (operand.typeId() == LogicalTypeId::BIGINT) return Value::bigint(-operand.asInt64());
                if (operand.typeId() == LogicalTypeId::INTEGER) return Value::integer(-operand.asInt32());
            }
            return operand;
        }

        default:
            return Value::null();
    }
}

// =============================================================================
// Aggregate Validation Methods
// =============================================================================

ExecutionResult Executor::validateAggregateQuery(const parser::SelectStmt& stmt,
                                                  const Schema& schema) const {
    // Collect all aggregates in the query
    auto aggregates = collectAggregates(stmt);

    // If no aggregates, no validation needed
    if (aggregates.empty() && !stmt.hasGroupBy()) {
        return ExecutionResult::empty();
    }

    // Validate type constraints for each aggregate (SUM/AVG require numeric)
    for (const auto* agg : aggregates) {
        std::string error_msg;
        if (!validateAggregateTypeConstraint(agg, schema, error_msg)) {
            return ExecutionResult::error(ErrorCode::TYPE_MISMATCH, error_msg);
        }
    }

    // If we have aggregates or GROUP BY, validate GROUP BY constraints
    bool has_aggs = !aggregates.empty();

    // Validate SELECT list: non-aggregate columns must be in GROUP BY
    for (const auto& col : stmt.columns()) {
        std::string error_msg;
        if (!validateExprGroupByConstraint(col.expr.get(), stmt, has_aggs, error_msg)) {
            return ExecutionResult::error(ErrorCode::SYNTAX_ERROR, error_msg);
        }
    }

    // Validate HAVING clause if present
    if (stmt.hasHaving()) {
        std::string error_msg;
        if (!validateExprGroupByConstraint(stmt.havingClause(), stmt, has_aggs, error_msg)) {
            return ExecutionResult::error(ErrorCode::SYNTAX_ERROR,
                                          "HAVING clause: " + error_msg);
        }
    }

    // Validate ORDER BY clause if present
    if (stmt.hasOrderBy()) {
        for (const auto& order_expr : stmt.orderBy()) {
            std::string error_msg;
            if (!validateExprGroupByConstraint(order_expr.expr.get(), stmt, has_aggs, error_msg)) {
                return ExecutionResult::error(ErrorCode::SYNTAX_ERROR,
                                              "ORDER BY clause: " + error_msg);
            }
        }
    }

    return ExecutionResult::empty();  // Validation passed
}

bool Executor::isColumnInGroupBy(const std::string& col_name,
                                  const parser::SelectStmt& stmt) const {
    if (!stmt.hasGroupBy()) {
        return false;
    }

    for (const auto& group_expr : stmt.groupBy()) {
        if (group_expr->type() == parser::NodeType::EXPR_COLUMN_REF) {
            const auto* col = static_cast<const parser::ColumnRef*>(group_expr.get());
            if (col->column() == col_name) {
                return true;
            }
        }
    }
    return false;
}

bool Executor::validateExprGroupByConstraint(const parser::Expr* expr,
                                              const parser::SelectStmt& stmt,
                                              bool has_aggregates,
                                              std::string& error_msg) const {
    if (!expr) return true;

    bool has_group_by = stmt.hasGroupBy();

    if (!has_aggregates && !has_group_by) {
        return true;
    }

    switch (expr->type()) {
        case parser::NodeType::EXPR_AGGREGATE:
            return true;

        case parser::NodeType::EXPR_COLUMN_REF: {
            const auto* col = static_cast<const parser::ColumnRef*>(expr);
            if (has_aggregates || has_group_by) {
                if (!isColumnInGroupBy(col->column(), stmt)) {
                    error_msg = "column '" + col->column() +
                                "' must appear in the GROUP BY clause or be used in an aggregate function";
                    return false;
                }
            }
            return true;
        }

        case parser::NodeType::EXPR_BINARY: {
            const auto* bin = static_cast<const parser::BinaryExpr*>(expr);
            if (!validateExprGroupByConstraint(bin->left(), stmt, has_aggregates, error_msg)) {
                return false;
            }
            return validateExprGroupByConstraint(bin->right(), stmt, has_aggregates, error_msg);
        }

        case parser::NodeType::EXPR_UNARY: {
            const auto* unary = static_cast<const parser::UnaryExpr*>(expr);
            return validateExprGroupByConstraint(unary->operand(), stmt, has_aggregates, error_msg);
        }

        case parser::NodeType::EXPR_LITERAL:
            return true;

        default:
            return true;
    }
}

bool Executor::validateAggregateTypeConstraint(const parser::AggregateExpr* agg,
                                                const Schema& schema,
                                                std::string& error_msg) const {
    if (!agg) return true;

    // COUNT doesn't have type constraints
    if (agg->aggType() == parser::AggregateType::COUNT) {
        return true;
    }

    // MIN/MAX work with any comparable type
    if (agg->aggType() == parser::AggregateType::MIN ||
        agg->aggType() == parser::AggregateType::MAX) {
        return true;
    }

    // SUM and AVG require numeric types
    if (agg->aggType() == parser::AggregateType::SUM ||
        agg->aggType() == parser::AggregateType::AVG) {
        const auto* arg = agg->arg();
        if (!arg) {
            error_msg = agg->functionName() + " requires an argument";
            return false;
        }

        // If argument is a column, check its type
        if (arg->type() == parser::NodeType::EXPR_COLUMN_REF) {
            const auto* col = static_cast<const parser::ColumnRef*>(arg);
            if (schema.hasColumn(col->column())) {
                LogicalTypeId type_id = schema.column(col->column()).type().id();
                if (type_id != LogicalTypeId::INTEGER &&
                    type_id != LogicalTypeId::DOUBLE) {
                    error_msg = agg->functionName() + " requires a numeric argument, but column '" +
                                col->column() + "' is not numeric";
                    return false;
                }
            }
        }
        // For other expression types (literals, binary), we assume they're valid
        // since they would fail at runtime if types don't match
    }

    return true;
}

// =============================================================================
// Aggregate Result Schema Methods
// =============================================================================

Schema Executor::computeAggregateResultSchema(const parser::SelectStmt& stmt,
                                               const Schema& source_schema,
                                               std::unordered_map<std::string, std::string>& alias_map) const {
    std::vector<ColumnSchema> result_columns;

    for (const auto& item : stmt.selectItems()) {
        const parser::Expr* expr = item.expr.get();
        std::string col_name;
        LogicalType col_type(LogicalTypeId::VARCHAR);

        // Check if alias is provided
        if (item.hasAlias()) {
            col_name = item.alias;
        }

        // Handle different expression types
        if (expr->type() == parser::NodeType::EXPR_AGGREGATE) {
            const auto* agg = static_cast<const parser::AggregateExpr*>(expr);

            // Generate name if no alias
            if (col_name.empty()) {
                col_name = generateAggregateName(agg);
            }

            // Infer type
            col_type = inferAggregateType(agg, source_schema);

            // Store alias mapping
            alias_map[col_name] = col_name;

        } else if (expr->type() == parser::NodeType::EXPR_COLUMN_REF) {
            const auto* col = static_cast<const parser::ColumnRef*>(expr);

            // Use column name if no alias
            if (col_name.empty()) {
                col_name = col->column();
            }

            // Get type from source schema
            if (source_schema.hasColumn(col->column())) {
                col_type = source_schema.column(col->column()).type();
            }

            // Store alias mapping
            alias_map[col_name] = col->column();
        } else {
            // For other expressions, use a generic name
            if (col_name.empty()) {
                col_name = "expr";
            }
        }

        result_columns.emplace_back(col_name, col_type, true);
    }

    return Schema(std::move(result_columns));
}

LogicalType Executor::inferAggregateType(const parser::AggregateExpr* agg,
                                          const Schema& source_schema) const {
    switch (agg->aggType()) {
        case parser::AggregateType::COUNT:
            // COUNT always returns INTEGER
            return LogicalType(LogicalTypeId::INTEGER);

        case parser::AggregateType::AVG:
            // AVG always returns DOUBLE
            return LogicalType(LogicalTypeId::DOUBLE);

        case parser::AggregateType::SUM: {
            // SUM returns same type as input (or DOUBLE if input is DOUBLE)
            const auto* arg = agg->arg();
            if (arg && arg->type() == parser::NodeType::EXPR_COLUMN_REF) {
                const auto* col = static_cast<const parser::ColumnRef*>(arg);
                if (source_schema.hasColumn(col->column())) {
                    LogicalTypeId type_id = source_schema.column(col->column()).type().id();
                    if (type_id == LogicalTypeId::DOUBLE) {
                        return LogicalType(LogicalTypeId::DOUBLE);
                    }
                }
            }
            // Default to INTEGER for SUM
            return LogicalType(LogicalTypeId::INTEGER);
        }

        case parser::AggregateType::MIN:
        case parser::AggregateType::MAX: {
            // MIN/MAX return same type as input
            const auto* arg = agg->arg();
            if (arg && arg->type() == parser::NodeType::EXPR_COLUMN_REF) {
                const auto* col = static_cast<const parser::ColumnRef*>(arg);
                if (source_schema.hasColumn(col->column())) {
                    return source_schema.column(col->column()).type();
                }
            }
            // Default to VARCHAR if can't determine
            return LogicalType(LogicalTypeId::VARCHAR);
        }

        default:
            return LogicalType(LogicalTypeId::VARCHAR);
    }
}

std::string Executor::generateAggregateName(const parser::AggregateExpr* agg) const {
    std::string name;

    // Get function name
    switch (agg->aggType()) {
        case parser::AggregateType::COUNT: name = "COUNT"; break;
        case parser::AggregateType::SUM:   name = "SUM";   break;
        case parser::AggregateType::AVG:   name = "AVG";   break;
        case parser::AggregateType::MIN:   name = "MIN";   break;
        case parser::AggregateType::MAX:   name = "MAX";   break;
    }

    name += "(";

    // Handle DISTINCT
    if (agg->isDistinct()) {
        name += "DISTINCT ";
    }

    // Handle argument
    if (agg->isStar()) {
        name += "*";
    } else if (agg->arg()) {
        // Get argument as string
        if (agg->arg()->type() == parser::NodeType::EXPR_COLUMN_REF) {
            const auto* col = static_cast<const parser::ColumnRef*>(agg->arg());
            name += col->column();
        } else {
            name += "expr";
        }
    }

    name += ")";
    return name;
}

// =============================================================================
// LIKE Pattern Matching
// =============================================================================

bool Executor::matchLikePattern(const std::string& str, const std::string& pattern) const {
    // Use dynamic programming for pattern matching
    // dp[i][j] = true if str[0..i-1] matches pattern[0..j-1]
    size_t m = str.size();
    size_t n = pattern.size();

    // dp[i][j] represents whether str[0..i) matches pattern[0..j)
    std::vector<std::vector<bool>> dp(m + 1, std::vector<bool>(n + 1, false));

    // Empty pattern matches empty string
    dp[0][0] = true;

    // Handle patterns that start with % - they can match empty string
    for (size_t j = 1; j <= n; ++j) {
        if (pattern[j - 1] == '%') {
            dp[0][j] = dp[0][j - 1];
        }
    }

    // Fill the DP table
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (pattern[j - 1] == '%') {
                // % matches zero or more characters
                // dp[i][j-1]: % matches zero characters
                // dp[i-1][j]: % matches one or more characters
                dp[i][j] = dp[i][j - 1] || dp[i - 1][j];
            } else if (pattern[j - 1] == '_') {
                // _ matches exactly one character
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                // Regular character match
                dp[i][j] = dp[i - 1][j - 1] && (str[i - 1] == pattern[j - 1]);
            }
        }
    }

    return dp[m][n];
}

// =============================================================================
// Result post-processing helpers
// =============================================================================

void Executor::applyDistinct(const parser::SelectStmt& stmt) {
    if (!stmt.isDistinct()) return;

    std::vector<Row> unique_rows;
    for (const auto& row : result_rows_) {
        bool is_duplicate = false;
        for (const auto& unique_row : unique_rows) {
            if (rowsEqual(row, unique_row)) {
                is_duplicate = true;
                break;
            }
        }
        if (!is_duplicate) {
            unique_rows.push_back(row);
        }
    }
    result_rows_ = std::move(unique_rows);
}

void Executor::applyLimitOffset(const parser::SelectStmt& stmt) {
    if (!stmt.hasLimit() && !stmt.hasOffset()) return;

    size_t offset = stmt.hasOffset() ? static_cast<size_t>(stmt.offset().value()) : 0;
    size_t limit = stmt.hasLimit() ? static_cast<size_t>(stmt.limit().value()) : result_rows_.size();

    if (offset >= result_rows_.size()) {
        result_rows_.clear();
    } else {
        size_t end_index = std::min(offset + limit, result_rows_.size());
        std::vector<Row> limited_rows;
        for (size_t i = offset; i < end_index; ++i) {
            limited_rows.push_back(std::move(result_rows_[i]));
        }
        result_rows_ = std::move(limited_rows);
    }
}

// =============================================================================
// Static helpers
// =============================================================================

double Executor::to_double(const Value& v) {
    switch (v.typeId()) {
        case LogicalTypeId::INTEGER: return static_cast<double>(v.asInt32());
        case LogicalTypeId::BIGINT:  return static_cast<double>(v.asInt64());
        case LogicalTypeId::FLOAT:   return static_cast<double>(v.asFloat());
        case LogicalTypeId::DOUBLE:  return v.asDouble();
        default: return 0.0;
    }
}

} // namespace seeddb
