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
    EXPR_AGGREGATE,
    EXPR_CASE,
    EXPR_IN,
    EXPR_BETWEEN,
    EXPR_LIKE,
    EXPR_COALESCE,
    EXPR_NULLIF,
    EXPR_FUNCTION_CALL,  // Scalar function call
    // Definitions
    COLUMN_DEF,
    TABLE_REF,
    JOIN_CLAUSE  // Join clause node
};

/// Aggregate function types
enum class AggregateType {
    COUNT,
    SUM,
    AVG,
    MIN,
    MAX
};

/// Join type enumeration
enum class JoinType {
    CROSS,      // CROSS JOIN or comma-separated
    INNER,      // INNER JOIN ... ON
    LEFT,       // LEFT [OUTER] JOIN ... ON
    RIGHT       // RIGHT [OUTER] JOIN ... ON
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

/// Aggregate function expression (COUNT, SUM, AVG, MIN, MAX)
class AggregateExpr : public Expr {
public:
    /// Construct an aggregate expression
    /// @param agg_type The aggregate function type
    /// @param arg The argument expression (nullptr for COUNT(*))
    /// @param distinct Whether DISTINCT modifier is used
    /// @param is_star Whether COUNT(*) was used
    AggregateExpr(AggregateType agg_type, std::unique_ptr<Expr> arg,
                  bool distinct = false, bool is_star = false)
        : agg_type_(agg_type), arg_(std::move(arg)),
          distinct_(distinct), is_star_(is_star) {}

    NodeType type() const override { return NodeType::EXPR_AGGREGATE; }
    std::string toString() const override;

    /// Get the aggregate function type
    AggregateType aggType() const { return agg_type_; }

    /// Get the argument expression (may be nullptr for COUNT(*))
    const Expr* arg() const { return arg_.get(); }

    /// Check if DISTINCT modifier is used
    bool isDistinct() const { return distinct_; }

    /// Check if this is COUNT(*)
    bool isStar() const { return is_star_; }

    /// Get the function name as string
    std::string functionName() const {
        switch (agg_type_) {
            case AggregateType::COUNT: return "COUNT";
            case AggregateType::SUM: return "SUM";
            case AggregateType::AVG: return "AVG";
            case AggregateType::MIN: return "MIN";
            case AggregateType::MAX: return "MAX";
        }
        return "UNKNOWN";
    }

private:
    AggregateType agg_type_;
    std::unique_ptr<Expr> arg_;
    bool distinct_;
    bool is_star_;
};

/// CASE WHEN clause (condition and result)
struct CaseWhenClause {
    std::unique_ptr<Expr> when_expr;  // Condition to evaluate
    std::unique_ptr<Expr> then_expr;  // Result if condition is true

    CaseWhenClause();
    CaseWhenClause(std::unique_ptr<Expr> when_e, std::unique_ptr<Expr> then_e);
    ~CaseWhenClause();

    // Move operations
    CaseWhenClause(CaseWhenClause&&) = default;
    CaseWhenClause& operator=(CaseWhenClause&&) = default;
    CaseWhenClause(const CaseWhenClause&) = delete;
    CaseWhenClause& operator=(const CaseWhenClause&) = delete;
};

/// CASE WHEN expression
class CaseExpr : public Expr {
public:
    CaseExpr() = default;

    NodeType type() const override { return NodeType::EXPR_CASE; }
    std::string toString() const override;

    const auto& whenClauses() const { return when_clauses_; }
    const Expr* elseExpr() const { return else_expr_.get(); }
    bool hasElse() const { return else_expr_ != nullptr; }

    void addWhenClause(CaseWhenClause clause) {
        when_clauses_.push_back(std::move(clause));
    }
    void setElse(std::unique_ptr<Expr> else_expr) {
        else_expr_ = std::move(else_expr);
    }

private:
    std::vector<CaseWhenClause> when_clauses_;
    std::unique_ptr<Expr> else_expr_;
};

/// IN expression (expr IN (val1, val2, ...))
class InExpr : public Expr {
public:
    InExpr(std::unique_ptr<Expr> expr, bool negated = false)
        : expr_(std::move(expr)), negated_(negated) {}

    NodeType type() const override { return NodeType::EXPR_IN; }
    std::string toString() const override;

    const Expr* expr() const { return expr_.get(); }
    const auto& values() const { return values_; }
    bool isNegated() const { return negated_; }

    void addValue(std::unique_ptr<Expr> val) {
        values_.push_back(std::move(val));
    }

private:
    std::unique_ptr<Expr> expr_;
    std::vector<std::unique_ptr<Expr>> values_;
    bool negated_;
};

/// BETWEEN expression (expr BETWEEN low AND high)
class BetweenExpr : public Expr {
public:
    BetweenExpr(std::unique_ptr<Expr> expr,
                std::unique_ptr<Expr> low,
                std::unique_ptr<Expr> high,
                bool negated = false)
        : expr_(std::move(expr)),
          low_(std::move(low)),
          high_(std::move(high)),
          negated_(negated) {}

    NodeType type() const override { return NodeType::EXPR_BETWEEN; }
    std::string toString() const override;

    const Expr* expr() const { return expr_.get(); }
    const Expr* low() const { return low_.get(); }
    const Expr* high() const { return high_.get(); }
    bool isNegated() const { return negated_; }

private:
    std::unique_ptr<Expr> expr_;
    std::unique_ptr<Expr> low_;
    std::unique_ptr<Expr> high_;
    bool negated_;
};

/// LIKE expression (str LIKE pattern)
class LikeExpr : public Expr {
public:
    LikeExpr(std::unique_ptr<Expr> str,
             std::unique_ptr<Expr> pattern,
             bool negated = false)
        : str_(std::move(str)),
          pattern_(std::move(pattern)),
          negated_(negated) {}

    NodeType type() const override { return NodeType::EXPR_LIKE; }
    std::string toString() const override;

    const Expr* str() const { return str_.get(); }
    const Expr* pattern() const { return pattern_.get(); }
    bool isNegated() const { return negated_; }

private:
    std::unique_ptr<Expr> str_;
    std::unique_ptr<Expr> pattern_;
    bool negated_;
};

/// COALESCE function (return first non-NULL argument)
class CoalesceExpr : public Expr {
public:
    CoalesceExpr() = default;

    NodeType type() const override { return NodeType::EXPR_COALESCE; }
    std::string toString() const override;

    const auto& args() const { return args_; }
    size_t argCount() const { return args_.size(); }

    void addArg(std::unique_ptr<Expr> arg) {
        args_.push_back(std::move(arg));
    }

private:
    std::vector<std::unique_ptr<Expr>> args_;
};

/// NULLIF function (return NULL if expr1 == expr2, else expr1)
class NullifExpr : public Expr {
public:
    NullifExpr(std::unique_ptr<Expr> expr1, std::unique_ptr<Expr> expr2)
        : expr1_(std::move(expr1)), expr2_(std::move(expr2)) {}

    NodeType type() const override { return NodeType::EXPR_NULLIF; }
    std::string toString() const override;

    const Expr* expr1() const { return expr1_.get(); }
    const Expr* expr2() const { return expr2_.get(); }

private:
    std::unique_ptr<Expr> expr1_;
    std::unique_ptr<Expr> expr2_;
};

/// Scalar function call expression (e.g., UPPER(name), LENGTH(str))
class FunctionCallExpr : public Expr {
public:
    explicit FunctionCallExpr(std::string name) : name_(std::move(name)) {}
    
    NodeType type() const override { return NodeType::EXPR_FUNCTION_CALL; }
    std::string toString() const override;
    
    const std::string& functionName() const { return name_; }
    const auto& args() const { return args_; }
    size_t argCount() const { return args_.size(); }
    
    void addArg(std::unique_ptr<Expr> arg) { args_.push_back(std::move(arg)); }
    
private:
    std::string name_;
    std::vector<std::unique_ptr<Expr>> args_;
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

/// Join clause (represents a table join)
class JoinClause : public ASTNode {
public:
    JoinClause(JoinType type, std::unique_ptr<TableRef> table, 
               std::unique_ptr<Expr> condition = nullptr)
        : join_type_(type), table_(std::move(table)), 
          condition_(std::move(condition)) {}
    
    NodeType type() const override { return NodeType::JOIN_CLAUSE; }
    std::string toString() const override;
 
    JoinType joinType() const { return join_type_; }
    const TableRef* table() const { return table_.get(); }
    const Expr* condition() const { return condition_.get(); }
    bool hasCondition() const { return condition_ != nullptr; }
    
private:
    JoinType join_type_;
    std::unique_ptr<TableRef> table_;
    std::unique_ptr<Expr> condition_;  // nullptr for CROSS JOIN
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
    const auto& groupBy() const { return group_by_; }
    bool hasGroupBy() const { return !group_by_.empty(); }
    const Expr* havingClause() const { return having_clause_.get(); }
    bool hasHaving() const { return having_clause_ != nullptr; }
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
    void addGroupBy(std::unique_ptr<Expr> expr) { group_by_.push_back(std::move(expr)); }
    void setHaving(std::unique_ptr<Expr> having) { having_clause_ = std::move(having); }
    void addOrderBy(OrderByItem item) { order_by_.push_back(std::move(item)); }
    void setLimit(int64_t limit) { limit_ = limit; }
    void setOffset(int64_t offset) { offset_ = offset; }
    
    // Join support
    const auto& joins() const { return joins_; }
    bool hasJoins() const { return !joins_.empty(); }
    void addJoin(std::unique_ptr<JoinClause> join) { 
        joins_.push_back(std::move(join)); 
    }

private:
    bool select_all_ = false;
    bool distinct_ = false;
    std::vector<SelectItem> select_items_;
    std::unique_ptr<TableRef> from_table_;
    std::unique_ptr<Expr> where_clause_;
    std::vector<std::unique_ptr<Expr>> group_by_;
    std::unique_ptr<Expr> having_clause_;
    std::vector<OrderByItem> order_by_;
    std::optional<int64_t> limit_;
    std::optional<int64_t> offset_;
    std::vector<std::unique_ptr<JoinClause>> joins_;  // Additional tables with join type
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
