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

/// Get string name for data type
std::string data_type_to_string(DataType type);

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_AST_H
