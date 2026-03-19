#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "parser/lexer.h"

using namespace seeddb::parser;
using Catch::Matchers::WithinRel;

// Helper function to get token from result
Token get_token(seeddb::Result<Token>&& result) {
    REQUIRE(result.is_ok());
    return std::move(result.value());
}

TEST_CASE("Lexer: Keywords", "[lexer]") {
    SECTION("Basic keywords") {
        Lexer lexer("SELECT FROM WHERE");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::WHERE);
    }

    SECTION("Case insensitive") {
        Lexer lexer("select Select SELECT");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
    }

    SECTION("DDL keywords") {
        Lexer lexer("CREATE DROP ALTER TABLE INDEX VIEW");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::CREATE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DROP);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::ALTER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::TABLE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INDEX);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::VIEW);
    }

    SECTION("DML keywords") {
        Lexer lexer("INSERT INTO UPDATE DELETE VALUES SET");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INSERT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTO);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::UPDATE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DELETE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::VALUES);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SET);
    }

    SECTION("JOIN keywords") {
        Lexer lexer("JOIN INNER LEFT RIGHT OUTER CROSS ON USING");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::JOIN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INNER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::LEFT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::RIGHT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::OUTER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::CROSS);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::ON);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::USING);
    }

    SECTION("GROUP BY / ORDER BY keywords") {
        Lexer lexer("GROUP BY ORDER ASC DESC HAVING");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::GROUP);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::BY);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::ORDER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::ASC);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DESC);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::HAVING);
    }

    SECTION("LIMIT/OFFSET keywords") {
        Lexer lexer("LIMIT OFFSET");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::LIMIT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::OFFSET);
    }

    SECTION("DISTINCT keyword") {
        Lexer lexer("SELECT DISTINCT FROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DISTINCT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }

    SECTION("Data type keywords") {
        Lexer lexer("INTEGER BIGINT SMALLINT FLOAT DOUBLE VARCHAR CHAR BOOLEAN");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTEGER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::BIGINT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SMALLINT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FLOAT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DOUBLE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::VARCHAR);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::CHAR);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::BOOLEAN);
    }

    SECTION("Data type aliases") {
        Lexer lexer("INT BOOL");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTEGER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::BOOLEAN);
    }

    SECTION("Constraint keywords") {
        Lexer lexer("PRIMARY KEY FOREIGN REFERENCES UNIQUE NOT NULL DEFAULT");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::PRIMARY);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::KEY);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FOREIGN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::REFERENCES);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::UNIQUE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::NOT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::NULL_LIT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DEFAULT);
    }

    SECTION("Logical keywords") {
        Lexer lexer("AND OR TRUE FALSE");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::AND);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::OR);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::TRUE_LIT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FALSE_LIT);
    }

    SECTION("Subquery keywords") {
        Lexer lexer("EXISTS IN BETWEEN LIKE IS AS");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::EXISTS);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::BETWEEN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::LIKE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IS);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::AS);
    }
}

TEST_CASE("Lexer: Identifiers", "[lexer]") {
    SECTION("Simple identifier") {
        Lexer lexer("table_name");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::IDENTIFIER);
        REQUIRE(std::get<std::string>(tok.value) == "table_name");
    }

    SECTION("Identifier with numbers") {
        Lexer lexer("col123");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::IDENTIFIER);
        REQUIRE(std::get<std::string>(tok.value) == "col123");
    }

    SECTION("Identifier starting with underscore") {
        Lexer lexer("_hidden");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::IDENTIFIER);
        REQUIRE(std::get<std::string>(tok.value) == "_hidden");
    }

    SECTION("Identifier containing underscore") {
        Lexer lexer("user_id");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::IDENTIFIER);
        REQUIRE(std::get<std::string>(tok.value) == "user_id");
    }
}

TEST_CASE("Lexer: Integer literals", "[lexer]") {
    SECTION("Simple integer") {
        Lexer lexer("12345");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::INTEGER_LIT);
        REQUIRE(std::get<int64_t>(tok.value) == 12345);
    }

    SECTION("Zero") {
        Lexer lexer("0");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::INTEGER_LIT);
        REQUIRE(std::get<int64_t>(tok.value) == 0);
    }

    SECTION("Large integer") {
        Lexer lexer("9223372036854775807");  // INT64_MAX
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::INTEGER_LIT);
        REQUIRE(std::get<int64_t>(tok.value) == 9223372036854775807LL);
    }
}

TEST_CASE("Lexer: Float literals", "[lexer]") {
    SECTION("Simple float") {
        Lexer lexer("3.14");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::FLOAT_LIT);
        REQUIRE_THAT(std::get<double>(tok.value), WithinRel(3.14, 0.001));
    }

    SECTION("Float with exponent") {
        Lexer lexer("1.5e10");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::FLOAT_LIT);
        REQUIRE_THAT(std::get<double>(tok.value), WithinRel(1.5e10, 0.01));
    }

    SECTION("Float with negative exponent") {
        Lexer lexer("2.5E-3");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::FLOAT_LIT);
        REQUIRE_THAT(std::get<double>(tok.value), WithinRel(2.5e-3, 0.01));
    }

    SECTION("Float with positive exponent") {
        Lexer lexer("1e+5");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::FLOAT_LIT);
        REQUIRE_THAT(std::get<double>(tok.value), WithinRel(1e5, 0.01));
    }

    SECTION("Small decimal") {
        Lexer lexer("0.001");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::FLOAT_LIT);
        REQUIRE_THAT(std::get<double>(tok.value), WithinRel(0.001, 1e-6));
    }
}

TEST_CASE("Lexer: String literals", "[lexer]") {
    SECTION("Simple string") {
        Lexer lexer("'hello world'");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::STRING_LIT);
        REQUIRE(std::get<std::string>(tok.value) == "hello world");
    }

    SECTION("Empty string") {
        Lexer lexer("''");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::STRING_LIT);
        REQUIRE(std::get<std::string>(tok.value) == "");
    }

    SECTION("String with escaped quote") {
        Lexer lexer("'it''s ok'");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::STRING_LIT);
        REQUIRE(std::get<std::string>(tok.value) == "it's ok");
    }

    SECTION("String with backslash") {
        Lexer lexer("'a\\nb\\tc'");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::STRING_LIT);
        REQUIRE(std::get<std::string>(tok.value) == "a\\nb\\tc");
    }
}

TEST_CASE("Lexer: Operators", "[lexer]") {
    SECTION("Arithmetic operators") {
        Lexer lexer("+ - * / %");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::PLUS);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::MINUS);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::STAR);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SLASH);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::PERCENT);
    }

    SECTION("Comparison operators") {
        Lexer lexer("= <> < > <= >=");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::EQ);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::NE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::LT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::GT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::LE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::GE);
    }

    SECTION("Not equal alternative") {
        Lexer lexer("!=");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::NE);
    }

    SECTION("Concatenation operator") {
        Lexer lexer("||");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::CONCAT);
    }
}

TEST_CASE("Lexer: Delimiters", "[lexer]") {
    Lexer lexer("( ) [ ] , ; .");
    REQUIRE(get_token(lexer.next_token()).type == TokenType::LPAREN);
    REQUIRE(get_token(lexer.next_token()).type == TokenType::RPAREN);
    REQUIRE(get_token(lexer.next_token()).type == TokenType::LBRACKET);
    REQUIRE(get_token(lexer.next_token()).type == TokenType::RBRACKET);
    REQUIRE(get_token(lexer.next_token()).type == TokenType::COMMA);
    REQUIRE(get_token(lexer.next_token()).type == TokenType::SEMICOLON);
    REQUIRE(get_token(lexer.next_token()).type == TokenType::DOT);
}

TEST_CASE("Lexer: Location tracking", "[lexer]") {
    SECTION("Single line") {
        Lexer lexer("SELECT id");
        auto tok1 = get_token(lexer.next_token());
        REQUIRE(tok1.loc.line == 1);
        REQUIRE(tok1.loc.column == 1);
        REQUIRE(tok1.loc.start == 0);
        REQUIRE(tok1.loc.length == 6);

        auto tok2 = get_token(lexer.next_token());
        REQUIRE(tok2.loc.line == 1);
        REQUIRE(tok2.loc.column == 8);
        REQUIRE(tok2.loc.start == 7);
        REQUIRE(tok2.loc.length == 2);
    }

    SECTION("Multiple lines") {
        Lexer lexer("SELECT\n  FROM");
        auto tok1 = get_token(lexer.next_token());
        REQUIRE(tok1.loc.line == 1);
        REQUIRE(tok1.loc.column == 1);

        auto tok2 = get_token(lexer.next_token());
        REQUIRE(tok2.loc.line == 2);
        REQUIRE(tok2.loc.column == 3);
    }
}

TEST_CASE("Lexer: Whitespace and comments", "[lexer]") {
    SECTION("Skip spaces") {
        Lexer lexer("SELECT   FROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }

    SECTION("Skip tabs") {
        Lexer lexer("SELECT\t\tFROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }

    SECTION("Skip newlines") {
        Lexer lexer("SELECT\n\nFROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }

    SECTION("Line comment with --") {
        Lexer lexer("SELECT -- this is a comment\nFROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }

    SECTION("Line comment with #") {
        Lexer lexer("SELECT # comment\nFROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }

    SECTION("Block comment") {
        Lexer lexer("SELECT /* multi\nline\ncomment */ FROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
    }
}

TEST_CASE("Lexer: Peek token", "[lexer]") {
    SECTION("Peek does not consume") {
        Lexer lexer("SELECT FROM");

        auto peeked = get_token(lexer.peek_token());
        REQUIRE(peeked.type == TokenType::SELECT);

        auto tok1 = get_token(lexer.next_token());
        REQUIRE(tok1.type == TokenType::SELECT);

        auto tok2 = get_token(lexer.next_token());
        REQUIRE(tok2.type == TokenType::FROM);
    }

    SECTION("Multiple peeks return same token") {
        Lexer lexer("SELECT");

        auto peek1 = get_token(lexer.peek_token());
        auto peek2 = get_token(lexer.peek_token());
        REQUIRE(peek1.type == TokenType::SELECT);
        REQUIRE(peek2.type == TokenType::SELECT);

        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::SELECT);
    }
}

TEST_CASE("Lexer: End of input", "[lexer]") {
    SECTION("Empty input") {
        Lexer lexer("");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::END_OF_INPUT);
    }

    SECTION("Whitespace only") {
        Lexer lexer("   \t\n  ");
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::END_OF_INPUT);
    }

    SECTION("After tokens") {
        Lexer lexer("SELECT");
        get_token(lexer.next_token());
        auto tok = get_token(lexer.next_token());
        REQUIRE(tok.type == TokenType::END_OF_INPUT);
    }

    SECTION("Multiple END_OF_INPUT") {
        Lexer lexer("SELECT");
        get_token(lexer.next_token());
        auto tok1 = get_token(lexer.next_token());
        REQUIRE(tok1.type == TokenType::END_OF_INPUT);
        auto tok2 = get_token(lexer.next_token());
        REQUIRE(tok2.type == TokenType::END_OF_INPUT);
    }
}

TEST_CASE("Lexer: Error handling", "[lexer]") {
    SECTION("Unexpected character @") {
        Lexer lexer("SELECT @ FROM");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::UNEXPECTED_CHARACTER);
    }

    SECTION("Unexpected character $") {
        Lexer lexer("$var");
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::UNEXPECTED_CHARACTER);
    }

    SECTION("Unterminated string") {
        Lexer lexer("'unclosed string");
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::UNTERMINATED_STRING);
    }

    SECTION("Unterminated string at end") {
        Lexer lexer("'x");
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::UNTERMINATED_STRING);
    }

    SECTION("Invalid exponent") {
        Lexer lexer("1ex");
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::INVALID_NUMBER);
    }

    SECTION("Single pipe") {
        Lexer lexer("|");
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::UNEXPECTED_CHARACTER);
    }

    SECTION("Bang without equals") {
        Lexer lexer("!");
        auto result = lexer.next_token();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == seeddb::ErrorCode::UNEXPECTED_CHARACTER);
    }
}

TEST_CASE("Lexer: Complex SQL statements", "[lexer]") {
    SECTION("Simple SELECT") {
        Lexer lexer("SELECT id, name FROM users WHERE id = 1;");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::COMMA);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::WHERE);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::EQ);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTEGER_LIT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SEMICOLON);
    }

    SECTION("SELECT with JOIN") {
        Lexer lexer("SELECT u.name, o.total FROM users u JOIN orders o ON u.id = o.user_id");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // u
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DOT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // name
        REQUIRE(get_token(lexer.next_token()).type == TokenType::COMMA);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // o
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DOT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // total
        REQUIRE(get_token(lexer.next_token()).type == TokenType::FROM);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // users
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // u
        REQUIRE(get_token(lexer.next_token()).type == TokenType::JOIN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // orders
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // o
        REQUIRE(get_token(lexer.next_token()).type == TokenType::ON);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // u
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DOT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // id
        REQUIRE(get_token(lexer.next_token()).type == TokenType::EQ);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // o
        REQUIRE(get_token(lexer.next_token()).type == TokenType::DOT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);  // user_id
    }

    SECTION("Multiple statements") {
        Lexer lexer("SELECT 1; INSERT INTO t VALUES (2);");
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SELECT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTEGER_LIT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SEMICOLON);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INSERT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTO);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::IDENTIFIER);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::VALUES);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::LPAREN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::INTEGER_LIT);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::RPAREN);
        REQUIRE(get_token(lexer.next_token()).type == TokenType::SEMICOLON);
    }
}

TEST_CASE("Lexer: has_more", "[lexer]") {
    SECTION("Initially true for non-empty input") {
        Lexer lexer("SELECT");
        REQUIRE(lexer.has_more());
    }

    SECTION("Initially false for empty input") {
        Lexer lexer("");
        REQUIRE_FALSE(lexer.has_more());
    }
}

TEST_CASE("Lexer: current_location", "[lexer]") {
    Lexer lexer("SELECT FROM");
    REQUIRE(lexer.current_location().line == 1);
    REQUIRE(lexer.current_location().column == 1);

    get_token(lexer.next_token());
    REQUIRE(lexer.current_location().line == 1);
    REQUIRE(lexer.current_location().column == 7);  // After SELECT
}

TEST_CASE("Lexer: Location to_string", "[lexer]") {
    Location loc{10, 5, 100, 6};
    REQUIRE(loc.to_string() == "10:5");
}
