#include "executor/executor.h"

#include <algorithm>

#include "common/value.h"
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
    const std::string& table_name = stmt.tableName();

    // Check if table exists
    if (!catalog_.hasTable(table_name)) {
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + table_name + "' not found"
        );
    }

    // Get the table
    Table* table = catalog_.getTable(table_name);
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

    // Insert the row into the table
    table->insert(std::move(row));

    return ExecutionResult::empty();
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

    Table* table = catalog_.getTable(table_name);
    const Schema& schema = table->schema();
    const parser::Expr* where_clause = stmt.whereClause();

    // Track rows to update and count of updated rows
    std::vector<size_t> rows_to_update;
    size_t update_count = 0;

    // Find matching rows
    for (size_t i = 0; i < table->rowCount(); ++i) {
        const Row& row = table->get(i);
        if (!where_clause || evaluateWhereClause(where_clause, row, schema)) {
            rows_to_update.push_back(i);
        }
    }

    // Update matching rows
    for (size_t idx : rows_to_update) {
        // Take a value copy to avoid stale reference after table->update()
        Row old_row = table->get(idx);

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
            // Find column index
            auto col_idx_opt = schema.columnIndex(col_name);
            if (!col_idx_opt.has_value()) {
                return ExecutionResult::error(
                    ErrorCode::COLUMN_NOT_FOUND,
                    "Column '" + col_name + "' not found in table '" + table_name + "'"
                );
            }

            size_t col_idx = col_idx_opt.value();

            // Evaluate the expression using the old row for column references
            Value new_value = evaluateExpr(expr.get(), old_row, schema);
            new_values[col_idx] = new_value;
        }

        // Create new row and validate
        Row new_row(std::move(new_values));
        if (!schema.validateRow(new_row)) {
            return ExecutionResult::error(
                ErrorCode::CONSTRAINT_VIOLATION,
                "Updated row violates schema constraints"
            );
        }

        // Update the row
        table->update(idx, std::move(new_row));
        update_count++;
    }

        // Store update count for CLI to display
    // For now, return empty result (CLI will show "UPDATE N" separately)
    (void)update_count;  // Will be used later for result display
    return ExecutionResult::empty();
}

ExecutionResult Executor::execute(const parser::DeleteStmt& stmt) {
    const std::string& table_name = stmt.tableName();

    // Check if table exists
    if (!catalog_.hasTable(table_name)) {
        return ExecutionResult::error(
            ErrorCode::TABLE_NOT_FOUND,
            "Table '" + table_name + "' not found"
        );
    }

    Table* table = catalog_.getTable(table_name);
    const Schema& schema = table->schema();
    const parser::Expr* where_clause = stmt.whereClause();

    // Find rows to delete (collect indices in ascending order)
    std::vector<size_t> rows_to_delete;
    for (size_t i = 0; i < table->rowCount(); ++i) {
        const Row& row = table->get(i);
        if (!where_clause || evaluateWhereClause(where_clause, row, schema)) {
            rows_to_delete.push_back(i);
        }
    }

    // Delete rows from end to start to avoid index shifting issues
    size_t delete_count = rows_to_delete.size();
    // Bulk delete in O(n) time using erase-remove idiom
    table->removeBulk(rows_to_delete);

    (void)delete_count;  // Will be used later for result display
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

    // Get the table
    current_table_ = catalog_.getTable(table_name);
    if (!current_table_) {
        return false;
    }

    const Schema& schema = current_table_->schema();

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

    for (size_t i = 0; i < current_table_->rowCount(); ++i) {
        const Row& row = current_table_->get(i);

        // Apply WHERE clause filter
        if (where_clause && !evaluateWhereClause(where_clause, row, schema)) {
            continue;
        }

        // Project row
        Row projected = projectRow(row, schema, stmt, alias_map);
        result_rows_.push_back(std::move(projected));
    }

    // Apply DISTINCT if specified
    if (stmt.isDistinct()) {
        // Remove duplicates
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

    // Apply ORDER BY if specified
    if (stmt.hasOrderBy()) {
        sortResultRows(stmt, alias_map, result_schema);
    }

    // Apply LIMIT and OFFSET
    if (stmt.hasLimit() || stmt.hasOffset()) {
        size_t offset = stmt.hasOffset() ? static_cast<size_t>(stmt.offset().value()) : 0;
        size_t limit = stmt.hasLimit() ? static_cast<size_t>(stmt.limit().value()) : result_rows_.size();

        // Apply offset
        if (offset >= result_rows_.size()) {
            result_rows_.clear();
        } else {
            // Apply limit after offset  TODO: Optimize this later, could do it earlier.
            size_t end_index = std::min(offset + limit, result_rows_.size());
            std::vector<Row> limited_rows;
            for (size_t i = offset; i < end_index; ++i) {
                limited_rows.push_back(std::move(result_rows_[i]));
            }
            result_rows_ = std::move(limited_rows);
        }
    }

    return true;
}

bool Executor::hasNext() const {
    return current_table_ != nullptr && current_row_index_ < result_rows_.size();
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
    current_table_ = nullptr;
    result_rows_.clear();
    current_row_index_ = 0;
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

            // Find column index in schema
            for (size_t i = 0; i < schema.columnCount(); ++i) {
                if (schema.column(i).name() == col_name) {
                    return row.get(i);
                }
            }

            // Column not found
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
            auto isNumericType = [](LogicalTypeId id) {
                return id == LogicalTypeId::INTEGER ||
                       id == LogicalTypeId::BIGINT ||
                       id == LogicalTypeId::FLOAT ||
                       id == LogicalTypeId::DOUBLE;
            };

            // Helper lambda to convert a value to double for numeric comparison
            auto toDouble = [](const Value& v) -> double {
                switch (v.typeId()) {
                    case LogicalTypeId::INTEGER: return static_cast<double>(v.asInt32());
                    case LogicalTypeId::BIGINT: return static_cast<double>(v.asInt64());
                    case LogicalTypeId::FLOAT: return static_cast<double>(v.asFloat());
                    case LogicalTypeId::DOUBLE: return v.asDouble();
                    default: return 0.0;
                }
            };

            // Comparison operators with type coercion for numeric types
            if (op == "=") {
                // If both are numeric, compare as doubles
                if (isNumericType(left.typeId()) && isNumericType(right.typeId())) {
                    return Value::boolean(toDouble(left) == toDouble(right));
                }
                return Value::boolean(left.equals(right));
            }
            if (op == "<>" || op == "!=") {
                // If both are numeric, compare as doubles
                if (isNumericType(left.typeId()) && isNumericType(right.typeId())) {
                    return Value::boolean(toDouble(left) != toDouble(right));
                }
                return Value::boolean(!left.equals(right));
            }
            if (op == "<") {
                // If both are numeric, compare as doubles
                if (isNumericType(left.typeId()) && isNumericType(right.typeId())) {
                    return Value::boolean(toDouble(left) < toDouble(right));
                }
                return Value::boolean(left.lessThan(right));
            }
            if (op == ">") {
                // If both are numeric, compare as doubles
                if (isNumericType(left.typeId()) && isNumericType(right.typeId())) {
                    return Value::boolean(toDouble(left) > toDouble(right));
                }
                return Value::boolean(right.lessThan(left));
            }
            if (op == "<=") {
                // If both are numeric, compare as doubles
                if (isNumericType(left.typeId()) && isNumericType(right.typeId())) {
                    double l = toDouble(left), r = toDouble(right);
                    return Value::boolean(l <= r);
                }
                return Value::boolean(left.lessThan(right) || left.equals(right));
            }
            if (op == ">=") {
                // If both are numeric, compare as doubles
                if (isNumericType(left.typeId()) && isNumericType(right.typeId())) {
                    double l = toDouble(left), r = toDouble(right);
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

} // namespace seeddb
