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

// ===== DDL Statement Tests =====

TEST_CASE("Parser: CREATE TABLE", "[parser]") {
    SECTION("Basic CREATE TABLE") {
        std::string sql = "CREATE TABLE users (id INT, name TEXT)";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto& stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_CREATE_TABLE);

        auto* create = static_cast<CreateTableStmt*>(stmt.get());
        REQUIRE(create->tableName() == "users");
        REQUIRE(create->columns().size() == 2);
    }

    SECTION("CREATE TABLE with various types") {
        std::string sql = "CREATE TABLE products (id INT NOT NULL, price FLOAT, name VARCHAR(100))";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* create = static_cast<CreateTableStmt*>(result.value().get());
        REQUIRE(create->columns().size() == 3);
        REQUIRE_FALSE(create->columns()[0]->isNullable());  // id NOT NULL
        REQUIRE(create->columns()[1]->isNullable());  // price (default nullable)
        REQUIRE(create->columns()[2]->dataType().has_length());  // VARCHAR(100)
    }
}

TEST_CASE("Parser: DROP TABLE", "[parser]") {
    SECTION("Basic DROP TABLE") {
        std::string sql = "DROP TABLE users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto& stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_DROP_TABLE);

        auto* drop = static_cast<DropTableStmt*>(stmt.get());
        REQUIRE(drop->tableName() == "users");
        REQUIRE_FALSE(drop->hasIfExists());
    }

    SECTION("DROP TABLE IF EXISTS") {
        std::string sql = "DROP TABLE IF EXISTS old_table";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* drop = static_cast<DropTableStmt*>(result.value().get());
        REQUIRE(drop->hasIfExists());
    }
}

// ===== DML Statement Tests =====

TEST_CASE("Parser: SELECT", "[parser]") {
    SECTION("SELECT *") {
        std::string sql = "SELECT * FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto& stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_SELECT);

        auto* select = static_cast<SelectStmt*>(stmt.get());
        REQUIRE(select->isSelectAll());
        REQUIRE(select->fromTable()->name() == "users");
    }

    SECTION("SELECT with columns") {
        std::string sql = "SELECT id, name FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE_FALSE(select->isSelectAll());
        REQUIRE(select->columns().size() == 2);
    }

    SECTION("SELECT with WHERE") {
        std::string sql = "SELECT id, name FROM users WHERE age > 18";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE_FALSE(select->isSelectAll());
        REQUIRE(select->columns().size() == 2);
        REQUIRE(select->hasWhere());
    }

    SECTION("SELECT with complex WHERE") {
        std::string sql = "SELECT * FROM users WHERE age > 18 AND status = 'active'";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());
    }
}

TEST_CASE("Parser: INSERT", "[parser]") {
    SECTION("INSERT with values") {
        std::string sql = "INSERT INTO users (id, name) VALUES (1, 'Alice')";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto& stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_INSERT);

        auto* insert = static_cast<InsertStmt*>(stmt.get());
        REQUIRE(insert->tableName() == "users");
        REQUIRE(insert->columns().size() == 2);
        REQUIRE(insert->values().size() == 2);
    }
}

TEST_CASE("Parser: UPDATE", "[parser]") {
    SECTION("UPDATE with SET") {
        std::string sql = "UPDATE users SET name = 'Bob' WHERE id = 1";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto& stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_UPDATE);

        auto* update = static_cast<UpdateStmt*>(stmt.get());
        REQUIRE(update->tableName() == "users");
        REQUIRE(update->assignments().size() == 1);
        REQUIRE(update->hasWhere());
    }
}

TEST_CASE("Parser: DELETE", "[parser]") {
    SECTION("DELETE with WHERE") {
        std::string sql = "DELETE FROM users WHERE id = 1";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto& stmt = result.value();
        REQUIRE(stmt->type() == NodeType::STMT_DELETE);

        auto* del = static_cast<DeleteStmt*>(stmt.get());
        REQUIRE(del->tableName() == "users");
        REQUIRE(del->hasWhere());
    }
}

// ===== Expression Parsing Tests =====

TEST_CASE("Parser: Expressions", "[parser]") {
    SECTION("Arithmetic expressions") {
        std::string sql = "SELECT * FROM t WHERE a + b * c > 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("Comparison expressions") {
        std::string sql = "SELECT * FROM t WHERE a = 1 AND b <> 2";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("Logical expressions") {
        std::string sql = "SELECT * FROM t WHERE a OR b AND c";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("NOT expression") {
        std::string sql = "SELECT * FROM t WHERE NOT a = 1";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("Parenthesized expressions") {
        std::string sql = "SELECT * FROM t WHERE (a OR b) AND c";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }
}
