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
        case TokenType::CREATE:
            return wrapStatement(parseCreateTable());
        case TokenType::DROP:
            return wrapStatement(parseDropTable());
        case TokenType::SELECT:
            return wrapStatement(parseSelect());
        case TokenType::INSERT:
            return wrapStatement(parseInsert());
        case TokenType::UPDATE:
            return wrapStatement(parseUpdate());
        case TokenType::DELETE:
            return wrapStatement(parseDelete());
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
    std::string table_name = std::move(std::get<std::string>(current_token_.value));
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
    std::string name = std::move(std::get<std::string>(current_token_.value));
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
    std::string name = std::move(std::get<std::string>(current_token_.value));
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

    // Optional DISTINCT
    if (match(TokenType::DISTINCT)) {
        stmt->setDistinct(true);
    }

    // Parse columns
    if (check(TokenType::STAR)) {
        stmt->setSelectAll(true);
        consume();
    } else {
        // Parse select list with optional aliases
        auto select_list = parseSelectList();
        if (!select_list.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(select_list.error());
        }
        for (auto& item : select_list.value()) {
            stmt->addSelectItem(std::move(item));
        }
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

    // Optional GROUP BY
    if (match(TokenType::GROUP)) {
        if (!match(TokenType::BY)) {
            return syntax_error<std::unique_ptr<SelectStmt>>("Expected BY after GROUP");
        }
        auto group_by = parseGroupByClause();
        if (!group_by.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(group_by.error());
        }
        for (auto& expr : group_by.value()) {
            stmt->addGroupBy(std::move(expr));
        }
    }

    // Optional HAVING
    if (match(TokenType::HAVING)) {
        auto having = parseExpression();
        if (!having.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(having.error());
        }
        stmt->setHaving(std::move(having.value()));
    }

    // Optional ORDER BY
    if (match(TokenType::ORDER)) {
        if (!match(TokenType::BY)) {
            return syntax_error<std::unique_ptr<SelectStmt>>("Expected BY after ORDER");
        }
        auto order_by = parseOrderByClause();
        if (!order_by.is_ok()) {
            return Result<std::unique_ptr<SelectStmt>>::err(order_by.error());
        }
        for (auto& item : order_by.value()) {
            stmt->addOrderBy(std::move(item));
        }
    }

    // Optional LIMIT/OFFSET
    if (match(TokenType::LIMIT)) {
        if (!check(TokenType::INTEGER_LIT)) {
            return syntax_error<std::unique_ptr<SelectStmt>>("Expected integer after LIMIT");
        }
        int64_t limit = std::get<int64_t>(current_token_.value);
        if (limit < 0) {
            return syntax_error<std::unique_ptr<SelectStmt>>("LIMIT must be non-negative");
        }
        consume();
        stmt->setLimit(limit);

        // Optional OFFSET
        if (match(TokenType::OFFSET)) {
            if (!check(TokenType::INTEGER_LIT)) {
                return syntax_error<std::unique_ptr<SelectStmt>>("Expected integer after OFFSET");
            }
            int64_t offset = std::get<int64_t>(current_token_.value);
            if (offset < 0) {
                return syntax_error<std::unique_ptr<SelectStmt>>("OFFSET must be non-negative");
            }
            consume();
            stmt->setOffset(offset);
        }
    }

    return Result<std::unique_ptr<SelectStmt>>::ok(std::move(stmt));
}

// Parse a single select item (expression with optional alias)
Result<SelectItem> Parser::parseSelectItem() {
    auto expr = parseExpression();
    if (!expr.is_ok()) {
        return Result<SelectItem>::err(expr.error());
    }

    std::string alias;
    // Optional AS alias or just alias
    if (match(TokenType::AS)) {
        if (!check(TokenType::IDENTIFIER)) {
            return syntax_error<SelectItem>("Expected alias after AS");
        }
        alias = std::get<std::string>(current_token_.value);
        consume();
    } else if (check(TokenType::IDENTIFIER)) {
        // Implicit alias (no AS keyword) - but only if it's not a keyword
        // Check if next token could be a keyword that starts a clause
        // We need to be careful here - only treat as alias if it's clearly not a clause keyword
        // For simplicity, we support implicit alias with IDENTIFIER only
        alias = std::get<std::string>(current_token_.value);
        consume();
    }

    return Result<SelectItem>::ok(SelectItem(std::move(expr.value()), std::move(alias)));
}

// Parse comma-separated select list
Result<std::vector<SelectItem>> Parser::parseSelectList() {
    std::vector<SelectItem> items;

    do {
        auto item = parseSelectItem();
        if (!item.is_ok()) {
            return Result<std::vector<SelectItem>>::err(item.error());
        }
        items.push_back(std::move(item.value()));
    } while (match(TokenType::COMMA));

    return Result<std::vector<SelectItem>>::ok(std::move(items));
}

// Parse ORDER BY clause (after ORDER BY keywords)
Result<std::vector<OrderByItem>> Parser::parseOrderByClause() {
    std::vector<OrderByItem> items;

    do {
        auto expr = parseExpression();
        if (!expr.is_ok()) {
            return Result<std::vector<OrderByItem>>::err(expr.error());
        }

        SortDirection direction = SortDirection::ASC;
        if (match(TokenType::DESC)) {
            direction = SortDirection::DESC;
        } else {
            match(TokenType::ASC);  // Optional ASC keyword
        }

        items.push_back(OrderByItem(std::move(expr.value()), direction));
    } while (match(TokenType::COMMA));

    return Result<std::vector<OrderByItem>>::ok(std::move(items));
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

        left = Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<BinaryExpr>(op, std::move(left.value()), std::move(right.value()))
        );
    }

    // Check for IS [NOT] NULL (postfix operator)
    if (check(TokenType::IS)) {
        consume();
        bool negated = false;
        if (match(TokenType::NOT)) {
            negated = true;
        }
        if (!match(TokenType::NULL_LIT)) {
            return syntax_error<std::unique_ptr<Expr>>("Expected NULL after IS");
        }
        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<IsNullExpr>(std::move(left.value()), negated)
        );
    }

    // Check for [NOT] BETWEEN/LIKE/IN (postfix operators)
    if (check(TokenType::NOT)) {
        consume();  // consume NOT

        if (check(TokenType::BETWEEN)) {
            return parseBetweenExpr(std::move(left.value()), true);
        } else if (check(TokenType::LIKE)) {
            return parseLikeExpr(std::move(left.value()), true);
        } else if (check(TokenType::IN)) {
            return parseInExpr(std::move(left.value()), true);
        } else {
            // Not NOT BETWEEN/LIKE/IN, put back NOT by returning error
            return syntax_error<std::unique_ptr<Expr>>("Expected BETWEEN, LIKE, or IN after NOT");
        }
    }

    // Check for BETWEEN (postfix operator)
    if (check(TokenType::BETWEEN)) {
        return parseBetweenExpr(std::move(left.value()));
    }

    // Check for LIKE (postfix operator)
    if (check(TokenType::LIKE)) {
        return parseLikeExpr(std::move(left.value()));
    }

    // Check for IN (postfix operator)
    if (check(TokenType::IN)) {
        return parseInExpr(std::move(left.value()));
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
    // Check for CASE expression
    if (check(TokenType::CASE)) {
        return parseCaseExpr();
    }

    // Check for COALESCE function
    if (check(TokenType::COALESCE)) {
        return parseCoalesceExpr();
    }

    // Check for NULLIF function
    if (check(TokenType::NULLIF)) {
        return parseNullifExpr();
    }

    // Check for aggregate functions (COUNT, SUM, AVG, MIN, MAX)
    if (check(TokenType::COUNT) || check(TokenType::SUM) || check(TokenType::AVG) ||
        check(TokenType::MIN) || check(TokenType::MAX)) {
        return parseAggregateExpr();
    }

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

    // Column reference or function call
    if (check(TokenType::IDENTIFIER)) {
        std::string name = std::get<std::string>(current_token_.value);
        consume();

        // Check for function call: IDENTIFIER '('
        if (match(TokenType::LPAREN)) {
            // This is a function call
            auto func_expr = std::make_unique<FunctionCallExpr>(std::move(name));
            
            // Parse arguments if not empty
            if (!check(TokenType::RPAREN)) {
                do {
                    auto arg = parseExpression();
                    if (!arg.is_ok()) {
                        return arg;
                    }
                    func_expr->addArg(std::move(arg.value()));
                } while (match(TokenType::COMMA));
            }
            
            // Expect ')'
            if (!match(TokenType::RPAREN)) {
                return syntax_error<std::unique_ptr<Expr>>("Expected ')' after function arguments");
            }
            
            return Result<std::unique_ptr<Expr>>::ok(std::move(func_expr));
        }

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
    std::string name = std::move(std::get<std::string>(current_token_.value));
    consume();

    // Optional alias - with AS keyword or implicit
    std::string alias;
    if (match(TokenType::AS)) {
        if (!check(TokenType::IDENTIFIER)) {
            return syntax_error<std::unique_ptr<TableRef>>("Expected alias after AS");
        }
        alias = std::move(std::get<std::string>(current_token_.value));
        consume();
    } else if (check(TokenType::IDENTIFIER)) {
        // Implicit alias (no AS keyword)
        alias = std::move(std::get<std::string>(current_token_.value));
        consume();
    }

    return Result<std::unique_ptr<TableRef>>::ok(
        std::make_unique<TableRef>(std::move(name), std::move(alias)));
}

// Parse aggregate function expression
Result<std::unique_ptr<Expr>> Parser::parseAggregateExpr() {
    // Determine aggregate type
    AggregateType agg_type;
    switch (current_token_.type) {
        case TokenType::COUNT: agg_type = AggregateType::COUNT; break;
        case TokenType::SUM: agg_type = AggregateType::SUM; break;
        case TokenType::AVG: agg_type = AggregateType::AVG; break;
        case TokenType::MIN: agg_type = AggregateType::MIN; break;
        case TokenType::MAX: agg_type = AggregateType::MAX; break;
        default:
            return syntax_error<std::unique_ptr<Expr>>("Expected aggregate function");
    }
    consume();  // consume function name

    // Expect (
    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected '(' after aggregate function");
    }

    // Check for COUNT(*)
    if (agg_type == AggregateType::COUNT && check(TokenType::STAR)) {
        consume();  // consume *
        if (!match(TokenType::RPAREN)) {
            return syntax_error<std::unique_ptr<Expr>>("Expected ')' after COUNT(*)");
        }
        return Result<std::unique_ptr<Expr>>::ok(
            std::make_unique<AggregateExpr>(agg_type, nullptr, false, true));
    }

    // Check for DISTINCT modifier
    bool is_distinct = false;
    if (match(TokenType::DISTINCT)) {
        is_distinct = true;
    }

    // Parse argument expression
    auto arg = parseExpression();
    if (!arg.is_ok()) {
        return arg;
    }

    // Expect )
    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected ')' after aggregate argument");
    }

    return Result<std::unique_ptr<Expr>>::ok(
        std::make_unique<AggregateExpr>(agg_type, std::move(arg.value()), is_distinct, false));
}

// Parse GROUP BY clause
Result<std::vector<std::unique_ptr<Expr>>> Parser::parseGroupByClause() {
    std::vector<std::unique_ptr<Expr>> group_by;

    do {
        auto expr = parseExpression();
        if (!expr.is_ok()) {
            return Result<std::vector<std::unique_ptr<Expr>>>::err(expr.error());
        }
        group_by.push_back(std::move(expr.value()));
    } while (match(TokenType::COMMA));

    return Result<std::vector<std::unique_ptr<Expr>>>::ok(std::move(group_by));
}

// ===== New Expression Parsing (Phase 2.3) =====

// Parse CASE WHEN expression
Result<std::unique_ptr<Expr>> Parser::parseCaseExpr() {
    // Expect CASE
    if (!match(TokenType::CASE)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected CASE");
    }

    auto case_expr = std::make_unique<CaseExpr>();

    // Parse WHEN clauses
    while (match(TokenType::WHEN)) {
        auto when_cond = parseExpression();
        if (!when_cond.is_ok()) {
            return when_cond;
        }

        if (!match(TokenType::THEN)) {
            return syntax_error<std::unique_ptr<Expr>>("Expected THEN after WHEN condition");
        }

        auto then_expr = parseExpression();
        if (!then_expr.is_ok()) {
            return then_expr;
        }

        case_expr->addWhenClause(CaseWhenClause(
            std::move(when_cond.value()),
            std::move(then_expr.value())
        ));
    }

    // Optional ELSE clause
    if (match(TokenType::ELSE)) {
        auto else_expr = parseExpression();
        if (!else_expr.is_ok()) {
            return else_expr;
        }
        case_expr->setElse(std::move(else_expr.value()));
    }

    // Expect END
    if (!match(TokenType::END)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected END after CASE expression");
    }

    return Result<std::unique_ptr<Expr>>::ok(std::move(case_expr));
}

// Parse IN expression (left IN (values...))
Result<std::unique_ptr<Expr>> Parser::parseInExpr(std::unique_ptr<Expr> left, bool negated) {
    // Expect IN (already confirmed by caller)
    consume();  // consume IN

    // Expect (
    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected '(' after IN");
    }

    auto in_expr = std::make_unique<InExpr>(std::move(left), negated);

    // Parse value list
    do {
        auto val = parseExpression();
        if (!val.is_ok()) {
            return val;
        }
        in_expr->addValue(std::move(val.value()));
    } while (match(TokenType::COMMA));

    // Expect )
    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected ')' after IN value list");
    }

    return Result<std::unique_ptr<Expr>>::ok(std::move(in_expr));
}

// Parse BETWEEN expression (left BETWEEN low AND high)
Result<std::unique_ptr<Expr>> Parser::parseBetweenExpr(std::unique_ptr<Expr> left, bool negated) {
    // Expect BETWEEN (already confirmed by caller)
    consume();  // consume BETWEEN

    auto low = parseAdditiveExpr();  // Parse lower bound (same precedence as additive)
    if (!low.is_ok()) {
        return low;
    }

    if (!match(TokenType::AND)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected AND in BETWEEN expression");
    }

    auto high = parseAdditiveExpr();  // Parse upper bound
    if (!high.is_ok()) {
        return high;
    }

    return Result<std::unique_ptr<Expr>>::ok(
        std::make_unique<BetweenExpr>(std::move(left), std::move(low.value()), std::move(high.value()), negated)
    );
}

// Parse LIKE expression (str LIKE pattern)
Result<std::unique_ptr<Expr>> Parser::parseLikeExpr(std::unique_ptr<Expr> left, bool negated) {
    // Expect LIKE (already confirmed by caller)
    consume();  // consume LIKE

    auto pattern = parsePrimaryExpr();  // Pattern is a primary expression (usually string literal)
    if (!pattern.is_ok()) {
        return pattern;
    }

    return Result<std::unique_ptr<Expr>>::ok(
        std::make_unique<LikeExpr>(std::move(left), std::move(pattern.value()), negated)
    );
}

// Parse COALESCE function
Result<std::unique_ptr<Expr>> Parser::parseCoalesceExpr() {
    // Expect COALESCE (already confirmed by caller)
    consume();  // consume COALESCE

    // Expect (
    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected '(' after COALESCE");
    }

    auto coalesce_expr = std::make_unique<CoalesceExpr>();

    // Parse arguments
    do {
        auto arg = parseExpression();
        if (!arg.is_ok()) {
            return arg;
        }
        coalesce_expr->addArg(std::move(arg.value()));
    } while (match(TokenType::COMMA));

    // Expect )
    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected ')' after COALESCE arguments");
    }

    return Result<std::unique_ptr<Expr>>::ok(std::move(coalesce_expr));
}

// Parse NULLIF function
Result<std::unique_ptr<Expr>> Parser::parseNullifExpr() {
    // Expect NULLIF (already confirmed by caller)
    consume();  // consume NULLIF

    // Expect (
    if (!match(TokenType::LPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected '(' after NULLIF");
    }

    auto expr1 = parseExpression();
    if (!expr1.is_ok()) {
        return expr1;
    }

    if (!match(TokenType::COMMA)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected ',' between NULLIF arguments");
    }

    auto expr2 = parseExpression();
    if (!expr2.is_ok()) {
        return expr2;
    }

    // Expect )
    if (!match(TokenType::RPAREN)) {
        return syntax_error<std::unique_ptr<Expr>>("Expected ')' after NULLIF arguments");
    }

    return Result<std::unique_ptr<Expr>>::ok(
        std::make_unique<NullifExpr>(std::move(expr1.value()), std::move(expr2.value()))
    );
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
