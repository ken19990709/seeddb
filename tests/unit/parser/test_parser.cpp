// tests/unit/parser/test_parser.cpp
#include <catch2/catch_test_macros.hpp>
#include "parser/parser.h"
#include "parser/lexer.h"
#include "common/error.h"

using namespace seeddb::parser;
using namespace seeddb;

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

// ===== Error Handling Tests =====

TEST_CASE("Parser: Error handling", "[parser]") {
    SECTION("Syntax error - missing FROM") {
        std::string sql = "SELECT *";  // Missing FROM clause
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("Syntax error - unexpected token") {
        std::string sql = "INVALID KEYWORD";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("Syntax error - missing table name") {
        std::string sql = "SELECT * FROM";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("Syntax error - missing closing paren") {
        std::string sql = "SELECT * FROM t WHERE (a = 1";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("CREATE TABLE - missing column list") {
        std::string sql = "CREATE TABLE users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("INSERT - missing VALUES") {
        std::string sql = "INSERT INTO users (id) (1)";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("UPDATE - missing SET") {
        std::string sql = "UPDATE users name = 'test'";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.error().code() == ErrorCode::SYNTAX_ERROR);
    }

    SECTION("Error message is descriptive") {
        std::string sql = "SELECT * FORM users";  // FORM instead of FROM
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE_FALSE(result.is_ok());
        // Error message should mention what was expected
        REQUIRE_FALSE(result.error().message().empty());
    }
}

// ===== Result Set Operation Tests =====

TEST_CASE("Parser: SELECT with column alias", "[parser]") {
    SECTION("Column alias with AS") {
        std::string sql = "SELECT id AS user_id, name AS username FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->selectItems().size() == 2);
        REQUIRE(select->selectItems()[0].hasAlias());
        REQUIRE(select->selectItems()[0].alias == "user_id");
        REQUIRE(select->selectItems()[1].alias == "username");
    }

    SECTION("Column alias without AS") {
        std::string sql = "SELECT id user_id FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->selectItems().size() == 1);
        REQUIRE(select->selectItems()[0].hasAlias());
        REQUIRE(select->selectItems()[0].alias == "user_id");
    }
}

TEST_CASE("Parser: SELECT with table alias", "[parser]") {
    SECTION("Table alias with AS") {
        std::string sql = "SELECT t.id FROM users AS t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->fromTable()->hasAlias());
        REQUIRE(select->fromTable()->alias() == "t");
    }

    SECTION("Table alias without AS") {
        std::string sql = "SELECT t.id FROM users t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->fromTable()->hasAlias());
        REQUIRE(select->fromTable()->alias() == "t");
    }
}

TEST_CASE("Parser: SELECT DISTINCT", "[parser]") {
    SECTION("DISTINCT single column") {
        std::string sql = "SELECT DISTINCT name FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->isDistinct());
    }

    SECTION("DISTINCT multiple columns") {
        std::string sql = "SELECT DISTINCT dept, role FROM employees";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->isDistinct());
        REQUIRE(select->selectItems().size() == 2);
    }

    SECTION("DISTINCT *") {
        std::string sql = "SELECT DISTINCT * FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->isDistinct());
        REQUIRE(select->isSelectAll());
    }
}

TEST_CASE("Parser: ORDER BY", "[parser]") {
    SECTION("ORDER BY single column ASC") {
        std::string sql = "SELECT * FROM users ORDER BY name";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasOrderBy());
        REQUIRE(select->orderBy().size() == 1);
        REQUIRE(select->orderBy()[0].direction == SortDirection::ASC);
    }

    SECTION("ORDER BY single column DESC") {
        std::string sql = "SELECT * FROM users ORDER BY age DESC";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasOrderBy());
        REQUIRE(select->orderBy().size() == 1);
        REQUIRE(select->orderBy()[0].direction == SortDirection::DESC);
    }

    SECTION("ORDER BY multiple columns mixed directions") {
        std::string sql = "SELECT * FROM users ORDER BY dept ASC, age DESC";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->orderBy().size() == 2);
        REQUIRE(select->orderBy()[0].direction == SortDirection::ASC);
        REQUIRE(select->orderBy()[1].direction == SortDirection::DESC);
    }
}

TEST_CASE("Parser: LIMIT and OFFSET", "[parser]") {
    SECTION("LIMIT only") {
        std::string sql = "SELECT * FROM users LIMIT 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasLimit());
        REQUIRE(select->limit().value() == 10);
        REQUIRE_FALSE(select->hasOffset());
    }

    SECTION("LIMIT with OFFSET") {
        std::string sql = "SELECT * FROM users LIMIT 10 OFFSET 5";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasLimit());
        REQUIRE(select->limit().value() == 10);
        REQUIRE(select->hasOffset());
        REQUIRE(select->offset().value() == 5);
    }
}

TEST_CASE("Parser: Combined clauses", "[parser]") {
    SECTION("SELECT with WHERE, ORDER BY, LIMIT") {
        std::string sql = "SELECT * FROM users WHERE age > 18 ORDER BY name LIMIT 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());
        REQUIRE(select->hasOrderBy());
        REQUIRE(select->hasLimit());
    }

    SECTION("SELECT DISTINCT with ORDER BY and LIMIT") {
        std::string sql = "SELECT DISTINCT dept FROM employees ORDER BY dept LIMIT 5";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->isDistinct());
        REQUIRE(select->hasOrderBy());
        REQUIRE(select->hasLimit());
    }
}

// ===== Aggregate Function Tests =====

TEST_CASE("Parser: Aggregate expressions", "[parser]") {
    SECTION("COUNT(*)") {
        std::string sql = "SELECT COUNT(*) FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->selectItems().size() == 1);

        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::COUNT);
        REQUIRE(agg->isStar());
        REQUIRE_FALSE(agg->isDistinct());
    }

    SECTION("COUNT(column)") {
        std::string sql = "SELECT COUNT(name) FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::COUNT);
        REQUIRE_FALSE(agg->isStar());
        REQUIRE(agg->arg() != nullptr);
    }

    SECTION("COUNT(DISTINCT column)") {
        std::string sql = "SELECT COUNT(DISTINCT category) FROM products";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::COUNT);
        REQUIRE(agg->isDistinct());
        REQUIRE_FALSE(agg->isStar());
    }

    SECTION("SUM aggregate") {
        std::string sql = "SELECT SUM(amount) FROM orders";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::SUM);
    }

    SECTION("AVG aggregate") {
        std::string sql = "SELECT AVG(price) FROM products";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::AVG);
    }

    SECTION("MIN aggregate") {
        std::string sql = "SELECT MIN(age) FROM users";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::MIN);
    }

    SECTION("MAX aggregate") {
        std::string sql = "SELECT MAX(salary) FROM employees";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* agg = dynamic_cast<const AggregateExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(agg != nullptr);
        REQUIRE(agg->aggType() == AggregateType::MAX);
    }

    SECTION("Multiple aggregates in SELECT") {
        std::string sql = "SELECT COUNT(*), SUM(amount), AVG(amount) FROM orders";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->selectItems().size() == 3);
    }
}

TEST_CASE("Parser: GROUP BY clause", "[parser]") {
    SECTION("GROUP BY single column") {
        std::string sql = "SELECT category, COUNT(*) FROM products GROUP BY category";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasGroupBy());
        REQUIRE(select->groupBy().size() == 1);
    }

    SECTION("GROUP BY multiple columns") {
        std::string sql = "SELECT country, city, COUNT(*) FROM users GROUP BY country, city";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasGroupBy());
        REQUIRE(select->groupBy().size() == 2);
    }

    SECTION("GROUP BY with WHERE") {
        std::string sql = "SELECT status, COUNT(*) FROM orders WHERE amount > 100 GROUP BY status";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());
        REQUIRE(select->hasGroupBy());
    }
}

TEST_CASE("Parser: HAVING clause", "[parser]") {
    SECTION("HAVING with aggregate condition") {
        std::string sql = "SELECT category, COUNT(*) FROM products GROUP BY category HAVING COUNT(*) > 5";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasGroupBy());
        REQUIRE(select->hasHaving());
    }

    SECTION("HAVING with multiple conditions") {
        std::string sql = "SELECT dept, AVG(salary) FROM employees GROUP BY dept HAVING AVG(salary) > 50000 AND COUNT(*) >= 3";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasGroupBy());
        REQUIRE(select->hasHaving());
    }
}

TEST_CASE("Parser: Combined aggregate clauses", "[parser]") {
    SECTION("SELECT with GROUP BY, HAVING, ORDER BY, LIMIT") {
        std::string sql = "SELECT category, SUM(price) AS total FROM products WHERE active = true GROUP BY category HAVING SUM(price) > 100 ORDER BY total DESC LIMIT 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());
        REQUIRE(select->hasGroupBy());
        REQUIRE(select->hasHaving());
        REQUIRE(select->hasOrderBy());
        REQUIRE(select->hasLimit());
    }
}

// ===== CASE WHEN Expression Tests =====

TEST_CASE("Parser: CASE WHEN expression", "[parser][expression]") {
    SECTION("CASE WHEN single branch") {
        std::string sql = "SELECT CASE WHEN a > 0 THEN 1 END FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* case_expr = dynamic_cast<const CaseExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(case_expr != nullptr);
        REQUIRE(case_expr->whenClauses().size() == 1);
        REQUIRE_FALSE(case_expr->hasElse());
    }

    SECTION("CASE WHEN multiple branches") {
        std::string sql = "SELECT CASE WHEN a = 1 THEN 'one' WHEN a = 2 THEN 'two' WHEN a = 3 THEN 'three' END FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* case_expr = dynamic_cast<const CaseExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(case_expr != nullptr);
        REQUIRE(case_expr->whenClauses().size() == 3);
        REQUIRE_FALSE(case_expr->hasElse());
    }

    SECTION("CASE WHEN with ELSE clause") {
        std::string sql = "SELECT CASE WHEN a > 0 THEN 'positive' ELSE 'zero or negative' END FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* case_expr = dynamic_cast<const CaseExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(case_expr != nullptr);
        REQUIRE(case_expr->whenClauses().size() == 1);
        REQUIRE(case_expr->hasElse());
    }

    SECTION("CASE WHEN in WHERE clause") {
        std::string sql = "SELECT * FROM t WHERE CASE WHEN a > 0 THEN true ELSE false END";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());
    }
}

// ===== IN Expression Tests =====

TEST_CASE("Parser: IN expression", "[parser][expression]") {
    SECTION("IN with literals") {
        std::string sql = "SELECT * FROM t WHERE a IN (1, 2, 3)";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());

        auto* in_expr = dynamic_cast<const InExpr*>(select->whereClause());
        REQUIRE(in_expr != nullptr);
        REQUIRE_FALSE(in_expr->isNegated());
        REQUIRE(in_expr->values().size() == 3);
    }

    SECTION("IN with strings") {
        std::string sql = "SELECT * FROM t WHERE name IN ('Alice', 'Bob', 'Charlie')";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* in_expr = dynamic_cast<const InExpr*>(select->whereClause());
        REQUIRE(in_expr != nullptr);
        REQUIRE(in_expr->values().size() == 3);
    }

    SECTION("NOT IN") {
        std::string sql = "SELECT * FROM t WHERE a NOT IN (1, 2, 3)";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());

        auto* in_expr = dynamic_cast<const InExpr*>(select->whereClause());
        REQUIRE(in_expr != nullptr);
        REQUIRE(in_expr->isNegated());
    }
}

// ===== BETWEEN Expression Tests =====

TEST_CASE("Parser: BETWEEN expression", "[parser][expression]") {
    SECTION("BETWEEN with numbers") {
        std::string sql = "SELECT * FROM t WHERE a BETWEEN 1 AND 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());

        auto* between_expr = dynamic_cast<const BetweenExpr*>(select->whereClause());
        REQUIRE(between_expr != nullptr);
        REQUIRE_FALSE(between_expr->isNegated());
    }

    SECTION("NOT BETWEEN") {
        std::string sql = "SELECT * FROM t WHERE a NOT BETWEEN 1 AND 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());

        auto* between_expr = dynamic_cast<const BetweenExpr*>(select->whereClause());
        REQUIRE(between_expr != nullptr);
        REQUIRE(between_expr->isNegated());
    }
}

// ===== LIKE Expression Tests =====

TEST_CASE("Parser: LIKE expression", "[parser][expression]") {
    SECTION("LIKE with pattern") {
        std::string sql = "SELECT * FROM t WHERE name LIKE 'A%'";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());

        auto* like_expr = dynamic_cast<const LikeExpr*>(select->whereClause());
        REQUIRE(like_expr != nullptr);
        REQUIRE_FALSE(like_expr->isNegated());
    }

    SECTION("LIKE with underscore wildcard") {
        std::string sql = "SELECT * FROM t WHERE name LIKE '_ob'";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* like_expr = dynamic_cast<const LikeExpr*>(select->whereClause());
        REQUIRE(like_expr != nullptr);
    }

    SECTION("NOT LIKE") {
        std::string sql = "SELECT * FROM t WHERE name NOT LIKE '%test%'";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());

        auto* like_expr = dynamic_cast<const LikeExpr*>(select->whereClause());
        REQUIRE(like_expr != nullptr);
        REQUIRE(like_expr->isNegated());
    }
}

// ===== COALESCE Expression Tests =====

TEST_CASE("Parser: COALESCE function", "[parser][expression]") {
    SECTION("COALESCE with two args") {
        std::string sql = "SELECT COALESCE(a, 0) FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* coalesce_expr = dynamic_cast<const CoalesceExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(coalesce_expr != nullptr);
        REQUIRE(coalesce_expr->args().size() == 2);
    }

    SECTION("COALESCE with multiple args") {
        std::string sql = "SELECT COALESCE(a, b, c, 'default') FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* coalesce_expr = dynamic_cast<const CoalesceExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(coalesce_expr != nullptr);
        REQUIRE(coalesce_expr->args().size() == 4);
    }

    SECTION("COALESCE in WHERE clause") {
        std::string sql = "SELECT * FROM t WHERE COALESCE(a, 0) > 10";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        REQUIRE(select->hasWhere());
    }
}

// ===== NULLIF Expression Tests =====

TEST_CASE("Parser: NULLIF function", "[parser][expression]") {
    SECTION("NULLIF with two args") {
        std::string sql = "SELECT NULLIF(a, 0) FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* nullif_expr = dynamic_cast<const NullifExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(nullif_expr != nullptr);
        REQUIRE(nullif_expr->expr1() != nullptr);
        REQUIRE(nullif_expr->expr2() != nullptr);
    }

    SECTION("NULLIF with column references") {
        std::string sql = "SELECT NULLIF(a, b) FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());

        auto* select = static_cast<SelectStmt*>(result.value().get());
        auto* nullif_expr = dynamic_cast<const NullifExpr*>(select->selectItems()[0].expr.get());
        REQUIRE(nullif_expr != nullptr);
    }
}

// ===== Complex Expression Combination Tests =====

TEST_CASE("Parser: Complex expressions combined", "[parser][expression]") {
    SECTION("CASE WHEN with IN") {
        std::string sql = "SELECT CASE WHEN a IN (1, 2, 3) THEN 'small' ELSE 'large' END FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("COALESCE with BETWEEN") {
        std::string sql = "SELECT * FROM t WHERE COALESCE(a, 0) BETWEEN 1 AND 100";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("Multiple expression types in WHERE") {
        std::string sql = "SELECT * FROM t WHERE a IN (1, 2, 3) AND b BETWEEN 0 AND 10 AND c LIKE '%test%'";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }

    SECTION("CASE WHEN with COALESCE and NULLIF") {
        std::string sql = "SELECT CASE WHEN NULLIF(a, 0) IS NULL THEN 'zero' ELSE COALESCE(b, 'unknown') END FROM t";
        Lexer lexer(sql);
        Parser parser(lexer);

        auto result = parser.parse();
        REQUIRE(result.is_ok());
    }
}
