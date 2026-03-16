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
    return Result<std::unique_ptr<Stmt>>::err(
        ErrorCode::NOT_IMPLEMENTED, "Parser::parse() not yet implemented");
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
