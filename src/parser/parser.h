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

    // DML statement parsing
    Result<std::unique_ptr<SelectStmt>> parseSelect();
    Result<std::unique_ptr<InsertStmt>> parseInsert();
    Result<std::unique_ptr<UpdateStmt>> parseUpdate();
    Result<std::unique_ptr<DeleteStmt>> parseDelete();

    // Expression parsing
    Result<std::unique_ptr<Expr>> parseExpression();
    Result<std::unique_ptr<Expr>> parseOrExpr();
    Result<std::unique_ptr<Expr>> parseAndExpr();
    Result<std::unique_ptr<Expr>> parseNotExpr();
    Result<std::unique_ptr<Expr>> parseComparisonExpr();
    Result<std::unique_ptr<Expr>> parseAdditiveExpr();
    Result<std::unique_ptr<Expr>> parseMultiplicativeExpr();
    Result<std::unique_ptr<Expr>> parseUnaryExpr();
    Result<std::unique_ptr<Expr>> parsePrimaryExpr();

    // Table reference
    Result<std::unique_ptr<TableRef>> parseTableRef();

    // Join clause parsing
    Result<std::unique_ptr<JoinClause>> parseJoinClause();

    // Column parsing
    Result<std::vector<std::unique_ptr<ColumnDef>>> parseColumnDefList();
    Result<std::unique_ptr<ColumnDef>> parseColumnDef();
    Result<DataTypeInfo> parseDataType();

    // SELECT clause parsing
    Result<SelectItem> parseSelectItem();
    Result<std::vector<SelectItem>> parseSelectList();
    Result<std::vector<OrderByItem>> parseOrderByClause();
    Result<std::pair<int64_t, int64_t>> parseLimitOffsetClause();  // returns {limit, offset}
    Result<std::vector<std::unique_ptr<Expr>>> parseGroupByClause();

    // Aggregate expression parsing
    Result<std::unique_ptr<Expr>> parseAggregateExpr();

    // New expression parsing (Phase 2.3)
    Result<std::unique_ptr<Expr>> parseCaseExpr();
    Result<std::unique_ptr<Expr>> parseInExpr(std::unique_ptr<Expr> left, bool negated = false);
    Result<std::unique_ptr<Expr>> parseBetweenExpr(std::unique_ptr<Expr> left, bool negated = false);
    Result<std::unique_ptr<Expr>> parseLikeExpr(std::unique_ptr<Expr> left, bool negated = false);
    Result<std::unique_ptr<Expr>> parseCoalesceExpr();
    Result<std::unique_ptr<Expr>> parseNullifExpr();

    // Error helper with location info
    template<typename T>
    Result<T> syntax_error(const std::string& message) const {
        std::string full_msg = current_token_.loc.to_string() + ": " + message;
        return Result<T>::err(ErrorCode::SYNTAX_ERROR, full_msg);
    }

    // Helper to convert specific statement type to Stmt base
    template<typename StmtT>
    Result<std::unique_ptr<Stmt>> wrapStatement(Result<std::unique_ptr<StmtT>> result) {
        if (!result.is_ok()) {
            return Result<std::unique_ptr<Stmt>>::err(result.error());
        }
        return Result<std::unique_ptr<Stmt>>::ok(
            std::unique_ptr<Stmt>(result.value().release()));
    }
};

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_PARSER_H
