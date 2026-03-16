#include "parser/ast.h"

namespace seeddb {
namespace parser {

std::string LiteralExpr::toString() const {
    if (isNull()) return "NULL";
    if (isInt()) return std::to_string(asInt());
    if (isFloat()) return std::to_string(asFloat());
    if (isBool()) return asBool() ? "TRUE" : "FALSE";
    if (isString()) return "'" + asString() + "'";
    return "UNKNOWN";
}

std::string ColumnRef::toString() const {
    return fullName();
}

std::string BinaryExpr::toString() const {
    return "(" + (left_ ? left_->toString() : "null") + " " + op_ + " " +
           (right_ ? right_->toString() : "null") + ")";
}

std::string UnaryExpr::toString() const {
    return op_ + "(" + (operand_ ? operand_->toString() : "null") + ")";
}

std::string IsNullExpr::toString() const {
    return (expr_ ? expr_->toString() : "null") + (negated_ ? " IS NOT NULL" : " IS NULL");
}

std::string data_type_to_string(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::BIGINT: return "BIGINT";
        case DataType::FLOAT: return "FLOAT";
        case DataType::DOUBLE: return "DOUBLE";
        case DataType::VARCHAR: return "VARCHAR";
        case DataType::TEXT: return "TEXT";
        case DataType::BOOLEAN: return "BOOLEAN";
        default: return "UNKNOWN";
    }
}

// ===== Statement Node Implementations =====

std::string ColumnDef::toString() const {
    std::string result = name_ + " " + data_type_to_string(data_type_.base_type_);
    if (data_type_.has_length()) {
        result += "(" + std::to_string(data_type_.length()) + ")";
    }
    if (!nullable_) result += " NOT NULL";
    return result;
}

std::string TableRef::toString() const {
    if (hasAlias()) return name_ + " AS " + alias_;
    return name_;
}

std::string CreateTableStmt::toString() const {
    std::string result = "CREATE TABLE " + table_name_ + " (";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0) result += ", ";
        result += columns_[i]->toString();
    }
    return result + ")";
}

std::string DropTableStmt::toString() const {
    std::string result = "DROP TABLE ";
    if (if_exists_) result += "IF EXISTS ";
    return result + table_name_;
}

std::string SelectStmt::toString() const {
    std::string result = "SELECT ";
    if (select_all_) {
        result += "*";
    } else {
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) result += ", ";
            result += columns_[i]->toString();
        }
    }
    if (from_table_) {
        result += " FROM " + from_table_->toString();
    }
    if (where_clause_) {
        result += " WHERE " + where_clause_->toString();
    }
    return result;
}

std::string InsertStmt::toString() const {
    std::string result = "INSERT INTO " + table_name_;
    if (!columns_.empty()) {
        result += " (";
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) result += ", ";
            result += columns_[i];
        }
        result += ")";
    }
    result += " VALUES (";
    for (size_t i = 0; i < values_.size(); ++i) {
        if (i > 0) result += ", ";
        result += values_[i]->toString();
    }
    return result + ")";
}

std::string UpdateStmt::toString() const {
    std::string result = "UPDATE " + table_name_ + " SET ";
    for (size_t i = 0; i < assignments_.size(); ++i) {
        if (i > 0) result += ", ";
        result += assignments_[i].first + " = " + assignments_[i].second->toString();
    }
    if (where_clause_) {
        result += " WHERE " + where_clause_->toString();
    }
    return result;
}

std::string DeleteStmt::toString() const {
    std::string result = "DELETE FROM " + table_name_;
    if (where_clause_) {
        result += " WHERE " + where_clause_->toString();
    }
    return result;
}

} // namespace parser
} // namespace seeddb
