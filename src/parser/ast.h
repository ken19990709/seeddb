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

/// Sort direction for ORDER BY
enum class SortDirection {
    ASC,
    DESC
};

// Forward declarations
class Expr;

/// ORDER BY item
struct OrderByItem {
    std::unique_ptr<Expr> expr;
    SortDirection direction = SortDirection::ASC;

    OrderByItem();
    OrderByItem(std::unique_ptr<Expr> e, SortDirection d = SortDirection::ASC);
    ~OrderByItem();

    // Move operations
    OrderByItem(OrderByItem&&) = default;
    OrderByItem& operator=(OrderByItem&&) = default;
    OrderByItem(const OrderByItem&) = delete;
    OrderByItem& operator=(const OrderByItem&) = delete;
};

/// SELECT item with optional alias
struct SelectItem {
    std::unique_ptr<Expr> expr;
    std::string alias;

    SelectItem();
    SelectItem(std::unique_ptr<Expr> e, std::string a = "");
    ~SelectItem();

    bool hasAlias() const { return !alias.empty(); }

    // Move operations
    SelectItem(SelectItem&&) = default;
    SelectItem& operator=(SelectItem&&) = default;
    SelectItem(const SelectItem&) = delete;
    SelectItem& operator=(const SelectItem&) = delete;
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

/// Column definition
class ColumnDef : public ASTNode {
public:
    ColumnDef(std::string name, DataTypeInfo data_type)
        : name_(std::move(name)), data_type_(std::move(data_type)) {}

    NodeType type() const override { return NodeType::COLUMN_DEF; }
    std::string toString() const override;

    const std::string& name() const { return name_; }
    const DataTypeInfo& dataType() const { return data_type_; }
    bool has_length() const { return data_type_.has_length(); }
    bool isNullable() const { return nullable_; }
    void setNullable(bool nullable) { nullable_ = nullable; }

private:
    std::string name_;
    DataTypeInfo data_type_;
    bool nullable_ = true;
};

/// Table reference
class TableRef : public ASTNode {
public:
    explicit TableRef(std::string name) : name_(std::move(name)) {}
    TableRef(std::string name, std::string alias)
        : name_(std::move(name)), alias_(std::move(alias)) {}

    NodeType type() const override { return NodeType::TABLE_REF; }
    std::string toString() const override;

    const std::string& name() const { return name_; }
    const std::string& alias() const { return alias_; }
    bool hasAlias() const { return !alias_.empty(); }

private:
    std::string name_;
    std::string alias_;
};

/// CREATE TABLE statement
class CreateTableStmt : public Stmt {
public:
    explicit CreateTableStmt(std::string table_name) : table_name_(std::move(table_name)) {}

    NodeType type() const override { return NodeType::STMT_CREATE_TABLE; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    const auto& columns() const { return columns_; }

    void addColumn(std::unique_ptr<ColumnDef> col) {
        columns_.push_back(std::move(col));
    }

private:
    std::string table_name_;
    std::vector<std::unique_ptr<ColumnDef>> columns_;
};

/// DROP TABLE statement
class DropTableStmt : public Stmt {
public:
    DropTableStmt(std::string table_name, bool if_exists = false)
        : table_name_(std::move(table_name)), if_exists_(if_exists) {}

    NodeType type() const override { return NodeType::STMT_DROP_TABLE; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    bool hasIfExists() const { return if_exists_; }

private:
    std::string table_name_;
    bool if_exists_;
};

/// SELECT statement
class SelectStmt : public Stmt {
public:
    NodeType type() const override { return NodeType::STMT_SELECT; }
    std::string toString() const override;

    bool isSelectAll() const { return select_all_; }
    bool isDistinct() const { return distinct_; }
    const auto& selectItems() const { return select_items_; }
    const TableRef* fromTable() const { return from_table_.get(); }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }
    const auto& orderBy() const { return order_by_; }
    bool hasOrderBy() const { return !order_by_.empty(); }
    const auto& limit() const { return limit_; }
    const auto& offset() const { return offset_; }
    bool hasLimit() const { return limit_.has_value(); }
    bool hasOffset() const { return offset_.has_value(); }

    // Legacy accessor for backward compatibility
    const auto& columns() const { return select_items_; }

    void setSelectAll(bool all) { select_all_ = all; }
    void setDistinct(bool distinct) { distinct_ = distinct; }
    void addSelectItem(SelectItem item) { select_items_.push_back(std::move(item)); }
    void addColumn(std::unique_ptr<Expr> col) {
        select_items_.push_back(SelectItem(std::move(col), ""));
    }
    void setFromTable(std::unique_ptr<TableRef> table) { from_table_ = std::move(table); }
    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }
    void addOrderBy(OrderByItem item) { order_by_.push_back(std::move(item)); }
    void setLimit(int64_t limit) { limit_ = limit; }
    void setOffset(int64_t offset) { offset_ = offset; }

private:
    bool select_all_ = false;
    bool distinct_ = false;
    std::vector<SelectItem> select_items_;
    std::unique_ptr<TableRef> from_table_;
    std::unique_ptr<Expr> where_clause_;
    std::vector<OrderByItem> order_by_;
    std::optional<int64_t> limit_;
    std::optional<int64_t> offset_;
};

/// INSERT statement
class InsertStmt : public Stmt {
public:
    explicit InsertStmt(std::string table_name) : table_name_(std::move(table_name)) {}

    NodeType type() const override { return NodeType::STMT_INSERT; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    const auto& columns() const { return columns_; }
    const auto& values() const { return values_; }

    void addColumn(std::string col) { columns_.push_back(std::move(col)); }
    void addValues(std::unique_ptr<Expr> val) { values_.push_back(std::move(val)); }

private:
    std::string table_name_;
    std::vector<std::string> columns_;
    std::vector<std::unique_ptr<Expr>> values_;
};

/// UPDATE statement
class UpdateStmt : public Stmt {
public:
    explicit UpdateStmt(std::string table_name) : table_name_(std::move(table_name)) {}

    NodeType type() const override { return NodeType::STMT_UPDATE; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    const auto& assignments() const { return assignments_; }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }

    void addAssignment(std::string column, std::unique_ptr<Expr> value) {
        assignments_.emplace_back(std::move(column), std::move(value));
    }
    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }

private:
    std::string table_name_;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> assignments_;
    std::unique_ptr<Expr> where_clause_;
};

/// DELETE statement
class DeleteStmt : public Stmt {
public:
    explicit DeleteStmt(std::string table_name) : table_name_(std::move(table_name)) {}

    NodeType type() const override { return NodeType::STMT_DELETE; }
    std::string toString() const override;

    const std::string& tableName() const { return table_name_; }
    const Expr* whereClause() const { return where_clause_.get(); }
    bool hasWhere() const { return where_clause_ != nullptr; }

    void setWhere(std::unique_ptr<Expr> where) { where_clause_ = std::move(where); }

private:
    std::string table_name_;
    std::unique_ptr<Expr> where_clause_;
};

/// Get string name for data type
std::string data_type_to_string(DataType type);

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_AST_H
