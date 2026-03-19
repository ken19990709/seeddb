#ifndef SEEDDB_PARSER_TOKEN_H
#define SEEDDB_PARSER_TOKEN_H

#include <string>
#include <variant>

namespace seeddb {
namespace parser {

/// Token types for SQL lexer
enum class TokenType {
    // ===== DDL =====
    CREATE, DROP, ALTER, TABLE, INDEX, VIEW,

    // ===== DML =====
    SELECT, FROM, WHERE, INSERT, INTO, UPDATE, DELETE,
    VALUES, SET,

    // ===== JOIN =====
    JOIN, INNER, LEFT, RIGHT, OUTER, CROSS, ON, USING,

    // ===== GROUP BY / ORDER BY =====
    GROUP, BY, ORDER, ASC, DESC, HAVING,
    LIMIT, OFFSET,
    DISTINCT,

    // ===== Subquery support =====
    EXISTS, IN, BETWEEN, LIKE, IS, AS, IF,

    // ===== Data types =====
    INTEGER, BIGINT, SMALLINT, FLOAT, DOUBLE, VARCHAR, CHAR, TEXT, BOOLEAN,

    // ===== Constraints =====
    PRIMARY, KEY, FOREIGN, REFERENCES, UNIQUE, NOT, NULL_LIT, DEFAULT,

    // ===== Logical operators =====
    AND, OR, TRUE_LIT, FALSE_LIT,

    // ===== Literals =====
    INTEGER_LIT,    // value: int64_t
    FLOAT_LIT,      // value: double
    STRING_LIT,     // value: std::string
    IDENTIFIER,     // value: std::string

    // ===== Operators =====
    PLUS, MINUS, STAR, SLASH, PERCENT,    // + - * / %
    EQ, NE, LT, GT, LE, GE,               // = <> < > <= >=
    CONCAT,                                 // ||

    // ===== Delimiters =====
    LPAREN, RPAREN,     // ( )
    LBRACKET, RBRACKET, // [ ]
    COMMA, SEMICOLON, DOT,

    // ===== Special =====
    END_OF_INPUT,
    ERROR
};

/// Get human-readable name for token type
const char* token_type_name(TokenType type);

/// Token value - stores the semantic value of a token
using TokenValue = std::variant<
    std::monostate,    // No value (keywords, operators, etc.)
    int64_t,           // Integer literal
    double,            // Float literal
    std::string,       // String literal / Identifier
    bool               // Boolean literal
>;

/// Source location for a token
struct Location {
    size_t line;       // Line number (1-based)
    size_t column;     // Column number (1-based)
    size_t start;      // Start position (0-based byte offset)
    size_t length;     // Token length in bytes

    std::string to_string() const;
};

/// Token - represents a lexical unit
struct Token {
    TokenType type;
    TokenValue value;
    Location loc;
};

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_TOKEN_H
