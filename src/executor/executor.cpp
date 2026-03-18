#include "executor/executor.h"

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

    // Find matching rows (apply WHERE clause)
    const parser::Expr* where_clause = stmt.whereClause();

    for (size_t i = 0; i < current_table_->rowCount(); ++i) {
        const Row& row = current_table_->get(i);

        // If no WHERE clause, all rows match
        if (!where_clause) {
            matching_rows_.push_back(i);
        } else {
            // Apply WHERE clause filter
            if (evaluateWhereClause(where_clause, row, schema)) {
                matching_rows_.push_back(i);
            }
        }
    }

    return true;
}

bool Executor::hasNext() const {
    return current_table_ != nullptr && current_row_index_ < matching_rows_.size();
}

ExecutionResult Executor::next() {
    if (!hasNext()) {
        return ExecutionResult::error(
            ErrorCode::INTERNAL_ERROR,
            "No more rows available"
        );
    }

    // Get the matching row
    size_t row_idx = matching_rows_[current_row_index_];
    const Row& row = current_table_->get(row_idx);

    // Advance to next row
    current_row_index_++;

    // Return a copy of the row
    return ExecutionResult::ok(Row(row));
}

void Executor::resetQuery() {
    current_table_ = nullptr;
    matching_rows_.clear();
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

} // namespace seeddb
