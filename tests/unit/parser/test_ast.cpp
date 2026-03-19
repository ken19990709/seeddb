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

TEST_CASE("AST: BinaryExpr", "[ast]") {
    SECTION("Arithmetic expression") {
        auto left = std::make_unique<LiteralExpr>(TokenValue{int64_t(1)});
        auto right = std::make_unique<LiteralExpr>(TokenValue{int64_t(2)});
        BinaryExpr expr("+", std::move(left), std::move(right));

        REQUIRE(expr.type() == NodeType::EXPR_BINARY);
        REQUIRE(expr.op() == "+");
        REQUIRE(expr.left() != nullptr);
        REQUIRE(expr.right() != nullptr);
        REQUIRE(expr.isArithmetic());
        REQUIRE_FALSE(expr.isComparison());
    }

    SECTION("Comparison expression") {
        auto left = std::make_unique<ColumnRef>("age");
        auto right = std::make_unique<LiteralExpr>(TokenValue{int64_t(18)});
        BinaryExpr expr(">", std::move(left), std::move(right));

        REQUIRE(expr.isComparison());
        REQUIRE_FALSE(expr.isArithmetic());
    }

    SECTION("Logical expression") {
        auto left = std::make_unique<ColumnRef>("active");
        auto right = std::make_unique<LiteralExpr>(TokenValue{true});
        BinaryExpr expr("AND", std::move(left), std::move(right));

        REQUIRE(expr.isLogical());
    }
}

TEST_CASE("AST: UnaryExpr", "[ast]") {
    SECTION("NOT expression") {
        auto operand = std::make_unique<LiteralExpr>(TokenValue{true});
        UnaryExpr expr("NOT", std::move(operand));

        REQUIRE(expr.type() == NodeType::EXPR_UNARY);
        REQUIRE(expr.op() == "NOT");
        REQUIRE(expr.operand() != nullptr);
        REQUIRE(expr.isNot());
        REQUIRE_FALSE(expr.isNegation());
    }

    SECTION("Negation expression") {
        auto operand = std::make_unique<LiteralExpr>(TokenValue{int64_t(42)});
        UnaryExpr expr("-", std::move(operand));

        REQUIRE(expr.isNegation());
        REQUIRE_FALSE(expr.isNot());
    }
}

TEST_CASE("AST: IsNullExpr", "[ast]") {
    SECTION("IS NULL") {
        auto expr = std::make_unique<ColumnRef>("email");
        IsNullExpr is_null(std::move(expr), false);

        REQUIRE(is_null.type() == NodeType::EXPR_IS_NULL);
        REQUIRE_FALSE(is_null.isNegated());
    }

    SECTION("IS NOT NULL") {
        auto expr = std::make_unique<ColumnRef>("phone");
        IsNullExpr is_not_null(std::move(expr), true);

        REQUIRE(is_not_null.isNegated());
    }
}

// ===== Statement Nodes Tests =====

TEST_CASE("AST: CreateTableStmt", "[ast]") {
    SECTION("Basic table creation") {
        CreateTableStmt stmt("users");

        REQUIRE(stmt.type() == NodeType::STMT_CREATE_TABLE);
        REQUIRE(stmt.tableName() == "users");
        REQUIRE(stmt.columns().empty());
    }

    SECTION("Table with columns") {
        CreateTableStmt stmt("users");

        auto col1 = std::make_unique<ColumnDef>("id", DataTypeInfo(DataType::INT));
        col1->setNullable(false);
        stmt.addColumn(std::move(col1));

        auto col2 = std::make_unique<ColumnDef>("name", DataTypeInfo(DataType::VARCHAR, 255));
        stmt.addColumn(std::move(col2));

        REQUIRE(stmt.columns().size() == 2);
        REQUIRE_FALSE(stmt.columns()[0]->isNullable());
        REQUIRE(stmt.columns()[1]->has_length());
    }
}

TEST_CASE("AST: ColumnDef", "[ast]") {
    SECTION("Simple column") {
        ColumnDef col("id", DataTypeInfo(DataType::INT));
        REQUIRE(col.type() == NodeType::COLUMN_DEF);
        REQUIRE(col.name() == "id");
        REQUIRE(col.dataType().base_type_ == DataType::INT);
        REQUIRE(col.isNullable());  // Default nullable
    }

    SECTION("VARCHAR with length") {
        ColumnDef col("name", DataTypeInfo(DataType::VARCHAR, 100));
        REQUIRE(col.dataType().has_length());
        REQUIRE(col.dataType().length() == 100);
    }

    SECTION("NOT NULL column") {
        ColumnDef col("id", DataTypeInfo(DataType::INT));
        col.setNullable(false);
        REQUIRE_FALSE(col.isNullable());
    }
}

TEST_CASE("AST: DropTableStmt", "[ast]") {
    SECTION("DROP TABLE") {
        DropTableStmt stmt("old_table");
        REQUIRE(stmt.type() == NodeType::STMT_DROP_TABLE);
        REQUIRE(stmt.tableName() == "old_table");
        REQUIRE_FALSE(stmt.hasIfExists());
    }

    SECTION("DROP TABLE IF EXISTS") {
        DropTableStmt stmt("old_table", true);
        REQUIRE(stmt.hasIfExists());
    }
}

TEST_CASE("AST: TableRef", "[ast]") {
    SECTION("Simple table reference") {
        TableRef ref("users");
        REQUIRE(ref.type() == NodeType::TABLE_REF);
        REQUIRE(ref.name() == "users");
        REQUIRE_FALSE(ref.hasAlias());
    }

    SECTION("Table with alias") {
        TableRef ref("users", "u");
        REQUIRE(ref.hasAlias());
        REQUIRE(ref.alias() == "u");
    }
}

TEST_CASE("AST: SelectStmt", "[ast]") {
    SECTION("SELECT *") {
        SelectStmt stmt;
        stmt.setSelectAll(true);

        REQUIRE(stmt.type() == NodeType::STMT_SELECT);
        REQUIRE(stmt.isSelectAll());
        REQUIRE_FALSE(stmt.hasWhere());
    }

    SECTION("SELECT with FROM and WHERE") {
        SelectStmt stmt;
        stmt.setSelectAll(true);
        stmt.setFromTable(std::make_unique<TableRef>("users"));

        auto where = std::make_unique<BinaryExpr>(
            "=",
            std::make_unique<ColumnRef>("id"),
            std::make_unique<LiteralExpr>(TokenValue{int64_t(1)})
        );
        stmt.setWhere(std::move(where));

        REQUIRE(stmt.fromTable() != nullptr);
        REQUIRE(stmt.hasWhere());
    }
}

TEST_CASE("AST: InsertStmt", "[ast]") {
    SECTION("Basic INSERT") {
        InsertStmt stmt("users");
        REQUIRE(stmt.type() == NodeType::STMT_INSERT);
        REQUIRE(stmt.tableName() == "users");
        REQUIRE(stmt.columns().empty());
        REQUIRE(stmt.values().empty());
    }

    SECTION("INSERT with columns and values") {
        InsertStmt stmt("users");
        stmt.addColumn("name");
        stmt.addValues(std::make_unique<LiteralExpr>(TokenValue{std::string("John")}));

        REQUIRE(stmt.columns().size() == 1);
        REQUIRE(stmt.values().size() == 1);
    }
}

TEST_CASE("AST: UpdateStmt", "[ast]") {
    SECTION("Basic UPDATE") {
        UpdateStmt stmt("users");
        REQUIRE(stmt.type() == NodeType::STMT_UPDATE);
        REQUIRE(stmt.tableName() == "users");
        REQUIRE_FALSE(stmt.hasWhere());
    }

    SECTION("UPDATE with SET and WHERE") {
        UpdateStmt stmt("users");
        stmt.addAssignment("name", std::make_unique<LiteralExpr>(TokenValue{std::string("Jane")}));

        auto where = std::make_unique<BinaryExpr>(
            "=",
            std::make_unique<ColumnRef>("id"),
            std::make_unique<LiteralExpr>(TokenValue{int64_t(1)})
        );
        stmt.setWhere(std::move(where));

        REQUIRE(stmt.assignments().size() == 1);
        REQUIRE(stmt.hasWhere());
    }
}

TEST_CASE("AST: DeleteStmt", "[ast]") {
    SECTION("Basic DELETE") {
        DeleteStmt stmt("users");
        REQUIRE(stmt.type() == NodeType::STMT_DELETE);
        REQUIRE(stmt.tableName() == "users");
        REQUIRE_FALSE(stmt.hasWhere());
    }

    SECTION("DELETE with WHERE") {
        DeleteStmt stmt("users");

        auto where = std::make_unique<BinaryExpr>(
            "=",
            std::make_unique<ColumnRef>("id"),
            std::make_unique<LiteralExpr>(TokenValue{int64_t(1)})
        );
        stmt.setWhere(std::move(where));

        REQUIRE(stmt.hasWhere());
    }
}

TEST_CASE("AST: SortDirection enum", "[ast]") {
    SECTION("ASC and DESC exist") {
        REQUIRE(static_cast<int>(SortDirection::ASC) == 0);
        REQUIRE(static_cast<int>(SortDirection::DESC) == 1);
    }
}

TEST_CASE("AST: OrderByItem", "[ast]") {
    SECTION("Default construction") {
        OrderByItem item;
        REQUIRE(item.expr == nullptr);
        REQUIRE(item.direction == SortDirection::ASC);
    }

    SECTION("With expression and ASC direction") {
        OrderByItem item(std::make_unique<ColumnRef>("name"), SortDirection::ASC);
        REQUIRE(item.expr != nullptr);
        REQUIRE(item.expr->type() == NodeType::EXPR_COLUMN_REF);
        REQUIRE(item.direction == SortDirection::ASC);
    }

    SECTION("With expression and DESC direction") {
        OrderByItem item(std::make_unique<ColumnRef>("age"), SortDirection::DESC);
        REQUIRE(item.expr != nullptr);
        REQUIRE(item.direction == SortDirection::DESC);
    }
}

TEST_CASE("AST: SelectItem", "[ast]") {
    SECTION("Default construction") {
        SelectItem item;
        REQUIRE(item.expr == nullptr);
        REQUIRE(item.alias.empty());
        REQUIRE_FALSE(item.hasAlias());
    }

    SECTION("With expression, no alias") {
        SelectItem item(std::make_unique<ColumnRef>("name"));
        REQUIRE(item.expr != nullptr);
        REQUIRE(item.expr->type() == NodeType::EXPR_COLUMN_REF);
        REQUIRE(item.alias.empty());
        REQUIRE_FALSE(item.hasAlias());
    }

    SECTION("With expression and alias") {
        SelectItem item(std::make_unique<ColumnRef>("name"), "username");
        REQUIRE(item.expr != nullptr);
        REQUIRE(item.hasAlias());
        REQUIRE(item.alias == "username");
    }
}

TEST_CASE("AST: SelectStmt with new fields", "[ast]") {
    SECTION("DISTINCT flag") {
        SelectStmt stmt;
        REQUIRE_FALSE(stmt.isDistinct());
        stmt.setDistinct(true);
        REQUIRE(stmt.isDistinct());
    }

    SECTION("ORDER BY items") {
        SelectStmt stmt;
        REQUIRE_FALSE(stmt.hasOrderBy());

        stmt.addOrderBy(OrderByItem(std::make_unique<ColumnRef>("name"), SortDirection::ASC));
        stmt.addOrderBy(OrderByItem(std::make_unique<ColumnRef>("age"), SortDirection::DESC));

        REQUIRE(stmt.hasOrderBy());
        REQUIRE(stmt.orderBy().size() == 2);
    }

    SECTION("LIMIT and OFFSET") {
        SelectStmt stmt;
        REQUIRE_FALSE(stmt.hasLimit());
        REQUIRE_FALSE(stmt.hasOffset());

        stmt.setLimit(100);
        stmt.setOffset(10);

        REQUIRE(stmt.hasLimit());
        REQUIRE(stmt.hasOffset());
        REQUIRE(stmt.limit().value() == 100);
        REQUIRE(stmt.offset().value() == 10);
    }

    SECTION("SelectItem with alias") {
        SelectStmt stmt;
        stmt.addSelectItem(SelectItem(std::make_unique<ColumnRef>("name"), "username"));

        REQUIRE(stmt.selectItems().size() == 1);
        REQUIRE(stmt.selectItems()[0].hasAlias());
        REQUIRE(stmt.selectItems()[0].alias == "username");
    }

    SECTION("toString includes new clauses") {
        SelectStmt stmt;
        stmt.setDistinct(true);
        stmt.addSelectItem(SelectItem(std::make_unique<ColumnRef>("name"), "username"));
        stmt.setFromTable(std::make_unique<TableRef>("users", "u"));
        stmt.addOrderBy(OrderByItem(std::make_unique<ColumnRef>("name"), SortDirection::DESC));
        stmt.setLimit(10);
        stmt.setOffset(5);

        std::string result = stmt.toString();
        REQUIRE(result.find("SELECT DISTINCT") != std::string::npos);
        REQUIRE(result.find("AS username") != std::string::npos);
        REQUIRE(result.find("ORDER BY") != std::string::npos);
        REQUIRE(result.find("DESC") != std::string::npos);
        REQUIRE(result.find("LIMIT 10") != std::string::npos);
        REQUIRE(result.find("OFFSET 5") != std::string::npos);
    }
}
