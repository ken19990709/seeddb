#ifndef SEEDDB_PARSER_PARSER_H
#define SEEDDB_PARSER_PARSER_H

#include <memory>
#include <vector>
#include "parser/lexer.h"
#include "parser/ast.h"
#include "common/error.h"

namespace seeddb {
namespace parser {

/// SQL parser - converts token stream to AST
class Parser {
public:
    /// Construct parser from lexer (takes reference)
    explicit Parser(Lexer& lexer);

    /// Parse single SQL statement
    Result<std::unique_ptr<Stmt>> parse();

    /// Parse multiple SQL statements (semicolon-separated)
    Result<std::vector<std::unique_ptr<Stmt>>> parseAll();

    /// Check if there are more tokens
    bool has_more() const;

    /// Get current token (lookahead, without consuming)
    const Token& current() const;

    /// Get current token type
    TokenType currentType() const;

    /// Consume and return current token
    Token consume();

    /// Expect specific token type, return error if mismatch
    Result<Token> expect(TokenType type, const char* message);

    /// Check if current token matches type
    bool check(TokenType type) const;

    /// Match and consume if type matches
    bool match(TokenType type);

private:
    Lexer& lexer_;
    Token current_token_;

    /// Advance to next token from lexer
    void advance();

    // Statement parsing
    Result<std::unique_ptr<Stmt>> parseStatement();
    Result<std::unique_ptr<CreateTableStmt>> parseCreateTable();
    Result<std::unique_ptr<DropTableStmt>> parseDropTable();

    // Column parsing
    Result<std::vector<std::unique_ptr<ColumnDef>>> parseColumnDefList();
    Result<std::unique_ptr<ColumnDef>> parseColumnDef();
    Result<DataTypeInfo> parseDataType();

    // Error helper
    template<typename T>
    Result<T> syntax_error(const std::string& message) const {
        return Result<T>::err(ErrorCode::SYNTAX_ERROR, message);
    }
};

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_PARSER_H
