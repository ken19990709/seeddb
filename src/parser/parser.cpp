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
        case TokenType::SELECT:
        case TokenType::INSERT:
        case TokenType::UPDATE:
        case TokenType::DELETE:
            return syntax_error<std::unique_ptr<Stmt>>("DML statements not yet implemented");
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
