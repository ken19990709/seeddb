// tests/unit/parser/test_ast.cpp
#include <catch2/catch_test_macros.hpp>
#include "parser/ast.h"

using namespace seeddb::parser;

TEST_CASE("AST: NodeType enum exists", "[ast]") {
    // Statement types
    REQUIRE(static_cast<int>(NodeType::STMT_CREATE_TABLE) == 0);
    REQUIRE(static_cast<int>(NodeType::STMT_DROP_TABLE) == 1);
    REQUIRE(static_cast<int>(NodeType::STMT_INSERT) == 2);
    REQUIRE(static_cast<int>(NodeType::STMT_SELECT) == 3);
    REQUIRE(static_cast<int>(NodeType::STMT_UPDATE) == 4);
    REQUIRE(static_cast<int>(NodeType::STMT_DELETE) == 5);

    // Expression types
    REQUIRE(static_cast<int>(NodeType::EXPR_BINARY) == 6);
    REQUIRE(static_cast<int>(NodeType::EXPR_UNARY) == 7);
    REQUIRE(static_cast<int>(NodeType::EXPR_LITERAL) == 8);
    REQUIRE(static_cast<int>(NodeType::EXPR_COLUMN_REF) == 9);
    REQUIRE(static_cast<int>(NodeType::EXPR_IS_NULL) == 10);

    // Definition types
    REQUIRE(static_cast<int>(NodeType::COLUMN_DEF) == 11);
    REQUIRE(static_cast<int>(NodeType::TABLE_REF) == 12);
}

TEST_CASE("AST: LiteralExpr", "[ast]") {
    SECTION("Integer literal") {
        LiteralExpr expr(TokenValue{int64_t(42)});
        REQUIRE(expr.type() == NodeType::EXPR_LITERAL);
        REQUIRE(std::holds_alternative<int64_t>(expr.value()));
        REQUIRE(std::get<int64_t>(expr.value()) == 42);
        REQUIRE_FALSE(expr.isNull());
    }

    SECTION("String literal") {
        LiteralExpr expr(TokenValue{std::string("hello")});
        REQUIRE(expr.type() == NodeType::EXPR_LITERAL);
        REQUIRE(std::holds_alternative<std::string>(expr.value()));
        REQUIRE(std::get<std::string>(expr.value()) == "hello");
    }

    SECTION("Null literal") {
        LiteralExpr expr(TokenValue{std::monostate{}});
        REQUIRE(expr.type() == NodeType::EXPR_LITERAL);
        REQUIRE(expr.isNull());
    }
}

TEST_CASE("AST: ColumnRef", "[ast]") {
    SECTION("Simple column reference") {
        ColumnRef ref("name");
        REQUIRE(ref.type() == NodeType::EXPR_COLUMN_REF);
        REQUIRE(ref.column() == "name");
        REQUIRE_FALSE(ref.hasTableQualifier());
        REQUIRE(ref.fullName() == "name");
    }

    SECTION("Qualified column reference") {
        ColumnRef ref("users", "id");
        REQUIRE(ref.hasTableQualifier());
        REQUIRE(ref.table() == "users");
        REQUIRE(ref.column() == "id");
        REQUIRE(ref.fullName() == "users.id");
    }
}
