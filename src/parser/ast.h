#ifndef SEEDDB_PARSER_AST_H
#define SEEDDB_PARSER_AST_H

#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "parser/token.h"

namespace seeddb {
namespace parser {

/// AST node types
enum class NodeType {
    // Statements
    STMT_CREATE_TABLE = 0,
    STMT_DROP_TABLE,
    STMT_INSERT,
    STMT_SELECT,
    STMT_UPDATE,
    STMT_DELETE,
    // Expressions
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LITERAL,
    EXPR_COLUMN_REF,
    EXPR_IS_NULL,
    // Definitions
    COLUMN_DEF,
    TABLE_REF
};

/// Data type enumeration
enum class DataType {
    INT,
    BIGINT,
    FLOAT,
    DOUBLE,
    VARCHAR,
    TEXT,
    BOOLEAN
};

/// Data type information with optional length
struct DataTypeInfo {
    DataType base_type_;
    std::optional<size_t> length_;

    DataTypeInfo() : base_type_(DataType::INT) {}
    explicit DataTypeInfo(DataType type) : base_type_(type) {}
    DataTypeInfo(DataType type, size_t len) : base_type_(type), length_(len) {}

    bool has_length() const { return length_.has_value(); }
    size_t length() const { return length_.value_or(0); }
};

/// AST node base class
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual NodeType type() const = 0;
    virtual std::string toString() const = 0;

    Location location() const { return location_; }
    void setLocation(Location loc) { location_ = loc; }

protected:
    Location location_;
};

/// Statement base class
class Stmt : public ASTNode {
};

/// Expression base class
class Expr : public ASTNode {
};

/// Literal expression (integer, float, string, bool, null)
class LiteralExpr : public Expr {
public:
    explicit LiteralExpr(TokenValue value) : value_(std::move(value)) {}
    LiteralExpr() : value_(std::monostate{}) {}  // Default: NULL

    NodeType type() const override { return NodeType::EXPR_LITERAL; }
    std::string toString() const override;

    const TokenValue& value() const { return value_; }
    bool isNull() const { return std::holds_alternative<std::monostate>(value_); }
    bool isInt() const { return std::holds_alternative<int64_t>(value_); }
    bool isFloat() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }

    int64_t asInt() const { return std::get<int64_t>(value_); }
    double asFloat() const { return std::get<double>(value_); }
    const std::string& asString() const { return std::get<std::string>(value_); }
    bool asBool() const { return std::get<bool>(value_); }

private:
    TokenValue value_;
};

/// Column reference expression
class ColumnRef : public Expr {
public:
    explicit ColumnRef(std::string column) : column_(std::move(column)) {}
    ColumnRef(std::string table, std::string column)
        : table_(std::move(table)), column_(std::move(column)) {}

    NodeType type() const override { return NodeType::EXPR_COLUMN_REF; }
    std::string toString() const override;

    const std::string& table() const { return table_; }
    const std::string& column() const { return column_; }
    bool hasTableQualifier() const { return !table_.empty(); }
    std::string fullName() const {
        return table_.empty() ? column_ : table_ + "." + column_;
    }

private:
    std::string table_;
    std::string column_;
};

/// Binary expression (arithmetic, comparison, logical)
class BinaryExpr : public Expr {
public:
    BinaryExpr(std::string op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : op_(std::move(op)), left_(std::move(left)), right_(std::move(right)) {}

    NodeType type() const override { return NodeType::EXPR_BINARY; }
    std::string toString() const override;

    const std::string& op() const { return op_; }
    const Expr* left() const { return left_.get(); }
    const Expr* right() const { return right_.get(); }

    bool isArithmetic() const {
        return op_ == "+" || op_ == "-" || op_ == "*" || op_ == "/" || op_ == "%";
    }
    bool isComparison() const {
        return op_ == "=" || op_ == "<>" || op_ == "<" || op_ == ">" ||
               op_ == "<=" || op_ == ">=";
    }
    bool isLogical() const { return op_ == "AND" || op_ == "OR"; }
    bool isConcat() const { return op_ == "||"; }

private:
    std::string op_;
    std::unique_ptr<Expr> left_;
    std::unique_ptr<Expr> right_;
};

/// Unary expression (NOT, -, +)
class UnaryExpr : public Expr {
public:
    UnaryExpr(std::string op, std::unique_ptr<Expr> operand)
        : op_(std::move(op)), operand_(std::move(operand)) {}

    NodeType type() const override { return NodeType::EXPR_UNARY; }
    std::string toString() const override;

    const std::string& op() const { return op_; }
    const Expr* operand() const { return operand_.get(); }
    bool isNot() const { return op_ == "NOT"; }
    bool isNegation() const { return op_ == "-"; }

private:
    std::string op_;
    std::unique_ptr<Expr> operand_;
};

/// IS NULL expression
class IsNullExpr : public Expr {
public:
    IsNullExpr(std::unique_ptr<Expr> expr, bool negated = false)
        : expr_(std::move(expr)), negated_(negated) {}

    NodeType type() const override { return NodeType::EXPR_IS_NULL; }
    std::string toString() const override;

    const Expr* expr() const { return expr_.get(); }
    bool isNegated() const { return negated_; }

private:
    std::unique_ptr<Expr> expr_;
    bool negated_;
};

/// Get string name for data type
std::string data_type_to_string(DataType type);

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_AST_H
