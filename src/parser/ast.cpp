#include "parser/ast.h"

namespace seeddb {
namespace parser {

// OrderByItem implementation
OrderByItem::OrderByItem() = default;
OrderByItem::OrderByItem(std::unique_ptr<Expr> e, SortDirection d)
    : expr(std::move(e)), direction(d) {}
OrderByItem::~OrderByItem() = default;

// SelectItem implementation
SelectItem::SelectItem() = default;
SelectItem::SelectItem(std::unique_ptr<Expr> e, std::string a)
    : expr(std::move(e)), alias(std::move(a)) {}
SelectItem::~SelectItem() = default;

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
    if (distinct_) {
        result += "DISTINCT ";
    }
    if (select_all_) {
        result += "*";
    } else {
        for (size_t i = 0; i < select_items_.size(); ++i) {
            if (i > 0) result += ", ";
            result += select_items_[i].expr->toString();
            if (select_items_[i].hasAlias()) {
                result += " AS " + select_items_[i].alias;
            }
        }
    }
    if (from_table_) {
        result += " FROM " + from_table_->toString();
    }
    if (where_clause_) {
        result += " WHERE " + where_clause_->toString();
    }
    if (!order_by_.empty()) {
        result += " ORDER BY ";
        for (size_t i = 0; i < order_by_.size(); ++i) {
            if (i > 0) result += ", ";
            result += order_by_[i].expr->toString();
            result += (order_by_[i].direction == SortDirection::DESC) ? " DESC" : " ASC";
        }
    }
    if (limit_.has_value()) {
        result += " LIMIT " + std::to_string(limit_.value());
    }
    if (offset_.has_value()) {
        result += " OFFSET " + std::to_string(offset_.value());
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
