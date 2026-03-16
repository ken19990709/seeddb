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

} // namespace parser
} // namespace seeddb
