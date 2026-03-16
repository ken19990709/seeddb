// tests/unit/parser/test_parser.cpp
#include <catch2/catch_test_macros.hpp>
#include "parser/parser.h"
#include "parser/lexer.h"

using namespace seeddb::parser;

TEST_CASE("Parser: construction", "[parser]") {
    Lexer lexer("SELECT 1");
    Parser parser(lexer);

    REQUIRE(parser.has_more());  // Should have tokens available
    REQUIRE(parser.current().type == TokenType::SELECT);
}

TEST_CASE("Parser: token manipulation", "[parser]") {
    Lexer lexer("SELECT id FROM users");
    Parser parser(lexer);

    // Initial state
    REQUIRE(parser.currentType() == TokenType::SELECT);
    REQUIRE(parser.check(TokenType::SELECT));
    REQUIRE_FALSE(parser.check(TokenType::INSERT));

    // Consume token
    Token tok = parser.consume();
    REQUIRE(tok.type == TokenType::SELECT);
    REQUIRE(parser.currentType() == TokenType::IDENTIFIER);

    // Match token
    REQUIRE(parser.match(TokenType::IDENTIFIER));
    REQUIRE(parser.currentType() == TokenType::FROM);

    // Failed match doesn't consume
    REQUIRE_FALSE(parser.match(TokenType::WHERE));
    REQUIRE(parser.currentType() == TokenType::FROM);
}

TEST_CASE("Parser: expect token", "[parser]") {
    Lexer lexer("SELECT id");
    Parser parser(lexer);

    // Successful expect
    auto result1 = parser.expect(TokenType::SELECT, "SELECT");
    REQUIRE(result1.is_ok());
    REQUIRE(result1.value().type == TokenType::SELECT);

    // Failed expect
    auto result2 = parser.expect(TokenType::FROM, "FROM");
    REQUIRE(!result2.is_ok());
}

TEST_CASE("Parser: has_more", "[parser]") {
    Lexer lexer("SELECT");
    Parser parser(lexer);

    REQUIRE(parser.has_more());
    parser.consume();
    REQUIRE_FALSE(parser.has_more());
}
