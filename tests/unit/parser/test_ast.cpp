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
