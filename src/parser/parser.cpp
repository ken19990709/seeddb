#include "parser/parser.h"
#include "parser/ast.h"

#include <sstream>

namespace seeddb {
namespace parser {

Parser::Parser(Lexer& lexer) : lexer_(lexer) {
    // Initialize current token from lexer
    advance();
}

void Parser::advance() {
    auto result = lexer_.next_token();
    if (result.is_ok()) {
        current_token_ = result.value();
    } else {
        current_token_ = Token{TokenType::END_OF_INPUT, std::monostate{}, Location{}};
    }
}

bool Parser::has_more() const {
    return current_token_.type != TokenType::END_OF_INPUT;
}

const Token& Parser::current() const {
    return current_token_;
}

TokenType Parser::currentType() const {
    return current_token_.type;
}

Token Parser::consume() {
    Token old = current_token_;
    advance();
    return old;
}

Result<Token> Parser::expect(TokenType type, const char* message) {
    if (current_token_.type != type) {
        return Result<Token>::err(ErrorCode::SYNTAX_ERROR,
            "Expected " + std::string(message) + ", got " +
            token_type_name(current_token_.type));
    }
    return Result<Token>::ok(consume());
}

bool Parser::check(TokenType type) const {
    return current_token_.type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        consume();
        return true;
    }
    return false;
}

// Placeholder implementation for parse()
Result<std::unique_ptr<Stmt>> Parser::parse() {
    return parseStatement();
}

Result<std::unique_ptr<Stmt>> Parser::parseStatement() {
    switch (current_token_.type) {
        case TokenType::CREATE: {
            auto result = parseCreateTable();
            if (!result.is_ok()) {
                return Result<std::unique_ptr<Stmt>>::err(result.error());
            }
            return Result<std::unique_ptr<Stmt>>::ok(
                std::unique_ptr<Stmt>(result.value().release()));
        }
        case TokenType::DROP: {
            auto result = parseDropTable();
            if (!result.is_ok()) {
                return Result<std::unique_ptr<Stmt>>::err(result.error());
            }
            return Result<std::unique_ptr<Stmt>>::ok(
                std::unique_ptr<Stmt>(result.value().release()));
        }
        case TokenType::SELECT: {
            auto result = parseSelect();
            if (!result.is_ok()) {
                return Result<std::unique_ptr<Stmt>>::err(result.error());
            }
            return Result<std::unique_ptr<Stmt>>::ok(
                std::unique_ptr<Stmt>(result.value().release()));
        }
        case TokenType::INSERT: {
            auto result = parseInsert();
            if (!result.is_ok()) {
                return Result<std::unique_ptr<Stmt>>::err(result.error());
            }
            return Result<std::unique_ptr<Stmt>>::ok(
                std::unique_ptr<Stmt>(result.value().release()));
        }
        case TokenType::UPDATE: {
            auto result = parseUpdate();
            if (!result.is_ok()) {
                return Result<std::unique_ptr<Stmt>>::err(result.error());
            }
            return Result<std::unique_ptr<Stmt>>::ok(
                std::unique_ptr<Stmt>(result.value().release()));
        }
        case TokenType::DELETE: {
            auto result = parseDelete();
            if (!result.is_ok()) {
                return Result<std::unique_ptr<Stmt>>::err(result.error());
            }
            return Result<std::unique_ptr<Stmt>>::ok(
                std::unique_ptr<Stmt>(result.value().release()));
        }
        default:
            return syntax_error<std::unique_ptr<Stmt>>(
                "Unexpected token: " + std::string(token_type_name(current_token_.type)));
    }
}

Result<std::unique_ptr<CreateTableStmt>> Parser::parseCreateTable() {
    // Expect CREATE
    if (!match(TokenType::CREATE)) {
        return syntax_error<std::unique_ptr<CreateTableStmt>>("Expected CREATE");
    }

    // Expect TABLE
    if (!match(TokenType::TABLE)) {
        return syntax_error<std::unique_ptr<CreateTableStmt>>("Expected TABLE");
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<CreateTableStmt>>("Expected table name");
    }
    std::string table_name = std::get<std::string>(current_token_.value);
    consume();

    // Expect (
    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<CreateTableStmt>>("Expected '('");
    }

    // Parse column definitions
    auto columns = parseColumnDefList();
    if (!columns.is_ok()) {
        return Result<std::unique_ptr<CreateTableStmt>>::err(columns.error());
    }

    // Expect )
    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<CreateTableStmt>>("Expected ')'");
    }

    auto stmt = std::make_unique<CreateTableStmt>(std::move(table_name));
    for (auto& col : columns.value()) {
        stmt->addColumn(std::move(col));
    }
    return Result<std::unique_ptr<CreateTableStmt>>::ok(std::move(stmt));
}

Result<std::unique_ptr<DropTableStmt>> Parser::parseDropTable() {
    // Expect DROP
    if (!match(TokenType::DROP)) {
        return syntax_error<std::unique_ptr<DropTableStmt>>("Expected DROP");
    }

    // Expect TABLE
    if (!match(TokenType::TABLE)) {
        return syntax_error<std::unique_ptr<DropTableStmt>>("Expected TABLE");
    }

    // Optional IF EXISTS
    bool if_exists = false;
    if (match(TokenType::IF)) {
        if (!match(TokenType::EXISTS)) {
            return syntax_error<std::unique_ptr<DropTableStmt>>("Expected EXISTS after IF");
        }
        if_exists = true;
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<DropTableStmt>>("Expected table name");
    }
    std::string name = std::get<std::string>(current_token_.value);
    consume();

    return Result<std::unique_ptr<DropTableStmt>>::ok(
        std::make_unique<DropTableStmt>(std::move(name), if_exists)
    );
}

Result<std::vector<std::unique_ptr<ColumnDef>>> Parser::parseColumnDefList() {
    std::vector<std::unique_ptr<ColumnDef>> columns;

    // Parse columns until we hit RPAREN
    while (!check(TokenType::RPAREN) && has_more()) {
        auto col = parseColumnDef();
        if (!col.is_ok()) {
            return Result<std::vector<std::unique_ptr<ColumnDef>>>::err(col.error());
        }
        columns.push_back(std::move(col.value()));

        // Optional comma
        if (check(TokenType::COMMA)) {
            consume();
        } else {
            break;  // No comma means end of list
        }
    }

    return Result<std::vector<std::unique_ptr<ColumnDef>>>::ok(std::move(columns));
}

Result<std::unique_ptr<ColumnDef>> Parser::parseColumnDef() {
    // Column name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<ColumnDef>>("Expected column name");
    }
    std::string name = std::get<std::string>(current_token_.value);
    consume();

    // Data type
    auto type_result = parseDataType();
    if (!type_result.is_ok()) {
        return Result<std::unique_ptr<ColumnDef>>::err(type_result.error());
    }

    auto col = std::make_unique<ColumnDef>(std::move(name), type_result.value());

    // Optional NOT NULL
    if (match(TokenType::NOT)) {
        if (!match(TokenType::NULL_LIT)) {
            return syntax_error<std::unique_ptr<ColumnDef>>("Expected NULL after NOT");
        }
        col->setNullable(false);
    }

    return Result<std::unique_ptr<ColumnDef>>::ok(std::move(col));
}

Result<DataTypeInfo> Parser::parseDataType() {
    DataType type;

    switch (current_token_.type) {
        case TokenType::INTEGER:
            type = DataType::INT;
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type));

        case TokenType::BIGINT:
            type = DataType::BIGINT;
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type));

        case TokenType::FLOAT:
            type = DataType::FLOAT;
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type));

        case TokenType::DOUBLE:
            type = DataType::DOUBLE;
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type));

        case TokenType::VARCHAR: {
            type = DataType::VARCHAR;
            size_t length = 0;
            consume();

            if (match(TokenType::LPAREN)) {
                if (!check(TokenType::INTEGER_LIT)) {
                    return syntax_error<DataTypeInfo>("Expected VARCHAR length");
                }
                length = static_cast<size_t>(std::get<int64_t>(current_token_.value));
                consume();

                if (!match(TokenType::RPAREN)) {
                    return syntax_error<DataTypeInfo>("Expected ')' after VARCHAR length");
                }
            }
            return Result<DataTypeInfo>::ok(DataTypeInfo(type, length));
        }

        case TokenType::TEXT:
            type = DataType::TEXT;
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type));

        case TokenType::BOOLEAN:
            type = DataType::BOOLEAN;
            consume();
            return Result<DataTypeInfo>::ok(DataTypeInfo(type));

        default:
            return syntax_error<DataTypeInfo>("Expected data type");
    }
}

// ===== DML Statement Parsing =====

Result<std::unique_ptr<SelectStmt>> Parser::parseSelect() {
    // Expect SELECT
    if (!match(TokenType::SELECT)) {
        return syntax_error<std::unique_ptr<SelectStmt>>("Expected SELECT");
    }

    auto stmt = std::make_unique<SelectStmt>();

    // Parse columns
    if (check(TokenType::STAR)) {
        stmt->setSelectAll(true);
        consume();
    } else {
        // Parse expression list
        do {
            auto expr = parseExpression();
            if (!expr.is_ok()) {
                return Result<std::unique_ptr<SelectStmt>>::err(expr.error());
            }
            stmt->addColumn(std::move(expr.value()));
        } while (match(TokenType::COMMA));
    }

    // FROM clause
    if (!match(TokenType::FROM)) {
        return syntax_error<std::unique_ptr<SelectStmt>>("Expected FROM");
    }

    auto table = parseTableRef();
    if (!table.is_ok()) {
        return Result<std::unique_ptr<SelectStmt>>::err(table.error());
    }
    stmt->setFromTable(std::move(table.value()));

    // Optional WHERE
    if (match(TokenType::WHERE)) {
        auto where = parseExpression();
        if (!where.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(where.error());
        }
        stmt->setWhere(std::move(where.value()));
    }

    return Result<std::unique_ptr<SelectStmt>>::ok(std::move(stmt));
}

Result<std::unique_ptr<InsertStmt>> Parser::parseInsert() {
    // Expect INSERT
    if (!match(TokenType::INSERT)) {
        return syntax_error<std::unique_ptr<InsertStmt>>("Expected INSERT");
    }

    // Expect INTO
    if (!match(TokenType::INTO)) {
        return syntax_error<std::unique_ptr<InsertStmt>>("Expected INTO");
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<InsertStmt>>("Expected table name");
    }
    auto stmt = std::make_unique<InsertStmt>(std::get<std::string>(current_token_.value));
    consume();

    // Optional column list
    if (match(TokenType::LPAREN)) {
        do {
            if (!check(TokenType::IDENTIFIER)) {
                return syntax_error<std::unique_ptr<InsertStmt>>("Expected column name");
            }
            stmt->addColumn(std::get<std::string>(current_token_.value));
            consume();
        } while (match(TokenType::COMMA));

        if (!match(TokenType::RPAREN)) {
            return syntax_error<std::unique_ptr<InsertStmt>>("Expected ')'");
        }
    }

    // VALUES
    if (!match(TokenType::VALUES)) {
        return syntax_error<std::unique_ptr<InsertStmt>>("Expected VALUES");
    }

    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<InsertStmt>>("Expected '('");
    }

    do {
        auto val = parseExpression();
        if (!val.is_ok()) {
            return Result<std::unique_ptr<InsertStmt>>::err(val.error());
        }
        stmt->addValues(std::move(val.value()));
    } while (match(TokenType::COMMA));

    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<InsertStmt>>("Expected ')'");
    }

    return Result<std::unique_ptr<InsertStmt>>::ok(std::move(stmt));
}

Result<std::unique_ptr<UpdateStmt>> Parser::parseUpdate() {
    // Expect UPDATE
    if (!match(TokenType::UPDATE)) {
        return syntax_error<std::unique_ptr<UpdateStmt>>("Expected UPDATE");
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<UpdateStmt>>("Expected table name");
    }
    auto stmt = std::make_unique<UpdateStmt>(std::get<std::string>(current_token_.value));
    consume();

    // Expect SET
    if (!match(TokenType::SET)) {
        return syntax_error<std::unique_ptr<UpdateStmt>>("Expected SET");
    }

    // Parse assignments
    do {
        if (!check(TokenType::IDENTIFIER)) {
            return syntax_error<std::unique_ptr<UpdateStmt>>("Expected column name");
        }
        std::string col = std::get<std::string>(current_token_.value);
        consume();

        if (!match(TokenType::EQ)) {
            return syntax_error<std::unique_ptr<UpdateStmt>>("Expected '='");
        }

        auto val = parseExpression();
        if (!val.is_ok()) {
            return Result<std::unique_ptr<UpdateStmt>>::err(val.error());
        }
        stmt->addAssignment(std::move(col), std::move(val.value()));
    } while (match(TokenType::COMMA));

    // Optional WHERE
    if (match(TokenType::WHERE)) {
        auto where = parseExpression();
        if (!where.is_ok()) {
            return Result<std::unique_ptr<UpdateStmt>>::err(where.error());
        }
        stmt->setWhere(std::move(where.value()));
    }

    return Result<std::unique_ptr<UpdateStmt>>::ok(std::move(stmt));
}

Result<std::unique_ptr<DeleteStmt>> Parser::parseDelete() {
    // Expect DELETE
    if (!match(TokenType::DELETE)) {
        return syntax_error<std::unique_ptr<DeleteStmt>>("Expected DELETE");
    }

    // Expect FROM
    if (!match(TokenType::FROM)) {
        return syntax_error<std::unique_ptr<DeleteStmt>>("Expected FROM");
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<DeleteStmt>>("Expected table name");
    }
    auto stmt = std::make_unique<DeleteStmt>(std::get<std::string>(current_token_.value));
    consume();

    // Optional WHERE
    if (match(TokenType::WHERE)) {
        auto where = parseExpression();
        if (!where.is_ok()) {
            return Result<std::unique_ptr<DeleteStmt>>::err(where.error());
        }
        stmt->setWhere(std::move(where.value()));
    }

    return Result<std::unique_ptr<DeleteStmt>>::ok(std::move(stmt));
}

// ===== Expression Parsing (Operator Precedence) =====

Result<std::unique_ptr<Expr>> Parser::parseExpression() {
    return parseOrExpr();
}

Result<std::unique_ptr<Expr>> Parser::parseOrExpr() {
    auto left = parseAndExpr();
    if (!left.is_ok()) return left;

    while (check(TokenType::OR)) {
        consume();
        auto right = parseAndExpr();
        if (!right.is_ok()) return right;

        left = Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>("OR", std::move(left.value()), std::move(right.value()))
        );
    }
    return left;
}

Result<std::unique_ptr<Expr>> Parser::parseAndExpr() {
    auto left = parseNotExpr();
    if (!left.is_ok()) return left;

    while (check(TokenType::AND)) {
        consume();
        auto right = parseNotExpr();
        if (!right.is_ok()) return right;

        left = Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>("AND", std::move(left.value()), std::move(right.value()))
        );
    }
    return left;
}

Result<std::unique_ptr<Expr>> Parser::parseNotExpr() {
    if (check(TokenType::NOT)) {
        consume();
        auto operand = parseNotExpr();
        if (!operand.is_ok()) return operand;

        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<UnaryExpr>("NOT", std::move(operand.value()))
        );
    }
    return parseComparisonExpr();
}

Result<std::unique_ptr<Expr>> Parser::parseComparisonExpr() {
    auto left = parseAdditiveExpr();
    if (!left.is_ok()) return left;

    // Check for comparison operators
    std::string op;
    if (check(TokenType::EQ)) op = "=";
    else if (check(TokenType::NE)) op = "<>";
    else if (check(TokenType::LT)) op = "<";
    else if (check(TokenType::GT)) op = ">";
    else if (check(TokenType::LE)) op = "<=";
    else if (check(TokenType::GE)) op = ">=";

    if (!op.empty()) {
        consume();
        auto right = parseAdditiveExpr();
        if (!right.is_ok()) return right;

        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>(op, std::move(left.value()), std::move(right.value()))
        );
    }

    return left;
}

Result<std::unique_ptr<Expr>> Parser::parseAdditiveExpr() {
    auto left = parseMultiplicativeExpr();
    if (!left.is_ok()) return left;

    while (check(TokenType::PLUS) || check(TokenType::MINUS) || check(TokenType::CONCAT)) {
        std::string op;
        if (check(TokenType::PLUS)) op = "+";
        else if (check(TokenType::MINUS)) op = "-";
        else op = "||";
        consume();

        auto right = parseMultiplicativeExpr();
        if (!right.is_ok()) return right;

        left = Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>(op, std::move(left.value()), std::move(right.value()))
        );
    }
    return left;
}

Result<std::unique_ptr<Expr>> Parser::parseMultiplicativeExpr() {
    auto left = parseUnaryExpr();
    if (!left.is_ok()) return left;

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        std::string op;
        if (check(TokenType::STAR)) op = "*";
        else if (check(TokenType::SLASH)) op = "/";
        else op = "%";
        consume();

        auto right = parseUnaryExpr();
        if (!right.is_ok()) return right;

        left = Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>(op, std::move(left.value()), std::move(right.value()))
        );
    }
    return left;
}

Result<std::unique_ptr<Expr>> Parser::parseUnaryExpr() {
    if (check(TokenType::MINUS) || check(TokenType::PLUS)) {
        std::string op = check(TokenType::MINUS) ? "-" : "+";
        consume();
        auto operand = parseUnaryExpr();
        if (!operand.is_ok()) return operand;

        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<UnaryExpr>(op, std::move(operand.value()))
        );
    }
    return parsePrimaryExpr();
}

Result<std::unique_ptr<Expr>> Parser::parsePrimaryExpr() {
    // Literals
    if (check(TokenType::INTEGER_LIT)) {
        auto val = TokenValue{std::get<int64_t>(current_token_.value)};
        consume();
        return Result<std::unique_ptr<Expr>>::ok(std::make_unique<LiteralExpr>(val));
    }
    if (check(TokenType::FLOAT_LIT)) {
        auto val = TokenValue{std::get<double>(current_token_.value)};
        consume();
        return Result<std::unique_ptr<Expr>>::ok(std::make_unique<LiteralExpr>(val));
    }
    if (check(TokenType::STRING_LIT)) {
        auto val = TokenValue{std::get<std::string>(current_token_.value)};
        consume();
        return Result<std::unique_ptr<Expr>>::ok(std::make_unique<LiteralExpr>(val));
    }
    if (check(TokenType::TRUE_LIT) || check(TokenType::FALSE_LIT)) {
        auto val = TokenValue{check(TokenType::TRUE_LIT)};
        consume();
        return Result<std::unique_ptr<Expr>>::ok(std::make_unique<LiteralExpr>(val));
    }
    if (check(TokenType::NULL_LIT)) {
        consume();
        return Result<std::unique_ptr<Expr>>::ok(std::make_unique<LiteralExpr>());
    }

    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        if (!expr.is_ok()) return expr;
        if (!match(TokenType::RPAREN)) {
            return syntax_error<std::unique_ptr<Expr>>("Expected ')'");
        }
        return expr;
    }

    // Column reference
    if (check(TokenType::IDENTIFIER)) {
        std::string name = std::get<std::string>(current_token_.value);
        consume();

        // Check for table.column
        if (match(TokenType::DOT)) {
            if (!check(TokenType::IDENTIFIER)) {
                return syntax_error<std::unique_ptr<Expr>>("Expected column name after '.'");
            }
            std::string col = std::get<std::string>(current_token_.value);
            consume();
            return Result<std::unique_ptr<Expr>>::ok(
                std::make_unique<ColumnRef>(std::move(name), std::move(col)));
        }

        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<ColumnRef>(std::move(name)));
    }

    return syntax_error<std::unique_ptr<Expr>>("Expected expression");
}

// ===== Table Reference Parsing =====

Result<std::unique_ptr<TableRef>> Parser::parseTableRef() {
    if (!check(TokenType::IDENTIFIER)) {
        return syntax_error<std::unique_ptr<TableRef>>("Expected table name");
    }
    std::string name = std::get<std::string>(current_token_.value);
    consume();

    // Optional alias
    std::string alias;
    if (match(TokenType::AS)) {
        if (!check(TokenType::IDENTIFIER)) {
            return syntax_error<std::unique_ptr<TableRef>>("Expected alias after AS");
        }
        alias = std::get<std::string>(current_token_.value);
        consume();
    }

    if (alias.empty()) {
        return Result<std::unique_ptr<TableRef>>::ok(std::make_unique<TableRef>(std::move(name)));
    }
    return Result<std::unique_ptr<TableRef>>::ok(
        std::make_unique<TableRef>(std::move(name), std::move(alias)));
}

// Placeholder implementation for parseAll()
Result<std::vector<std::unique_ptr<Stmt>>> Parser::parseAll() {
    std::vector<std::unique_ptr<Stmt>> statements;

    while (has_more()) {
        auto result = parse();
        if (!result.is_ok()) {
            return Result<std::vector<std::unique_ptr<Stmt>>>::err(result.error());
        }
        statements.push_back(std::move(result.value()));

        // Consume optional semicolon
        if (check(TokenType::SEMICOLON)) {
            consume();
        }
    }

    return Result<std::vector<std::unique_ptr<Stmt>>>::ok(std::move(statements));
}

} // namespace parser
} // namespace seeddb
