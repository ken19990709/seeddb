#ifndef SEEDDB_PARSER_KEYWORDS_H
#define SEEDDB_PARSER_KEYWORDS_H

#include <unordered_map>
#include <string_view>
#include "token.h"

namespace seeddb {
namespace parser {

/// SQL keywords lookup table (case-insensitive, use uppercase keys)
/// Using inline to avoid ODR violations (C++17)
inline const std::unordered_map<std::string_view, TokenType> keywords = {
    // DDL
    {"CREATE", TokenType::CREATE},
    {"DROP", TokenType::DROP},
    {"ALTER", TokenType::ALTER},
    {"TABLE", TokenType::TABLE},
    {"INDEX", TokenType::INDEX},
    {"VIEW", TokenType::VIEW},

    // DML
    {"SELECT", TokenType::SELECT},
    {"FROM", TokenType::FROM},
    {"WHERE", TokenType::WHERE},
    {"INSERT", TokenType::INSERT},
    {"INTO", TokenType::INTO},
    {"UPDATE", TokenType::UPDATE},
    {"DELETE", TokenType::DELETE},
    {"VALUES", TokenType::VALUES},
    {"SET", TokenType::SET},

    // JOIN
    {"JOIN", TokenType::JOIN},
    {"INNER", TokenType::INNER},
    {"LEFT", TokenType::LEFT},
    {"RIGHT", TokenType::RIGHT},
    {"OUTER", TokenType::OUTER},
    {"CROSS", TokenType::CROSS},
    {"ON", TokenType::ON},
    {"USING", TokenType::USING},

    // GROUP BY / ORDER BY
    {"GROUP", TokenType::GROUP},
    {"BY", TokenType::BY},
    {"ORDER", TokenType::ORDER},
    {"ASC", TokenType::ASC},
    {"DESC", TokenType::DESC},
    {"HAVING", TokenType::HAVING},

    // Subquery support
    {"EXISTS", TokenType::EXISTS},
    {"IN", TokenType::IN},
    {"BETWEEN", TokenType::BETWEEN},
    {"LIKE", TokenType::LIKE},
    {"IS", TokenType::IS},
    {"AS", TokenType::AS},
    {"IF", TokenType::IF},

    // Data types
    {"INTEGER", TokenType::INTEGER},
    {"INT", TokenType::INTEGER},  // Alias
    {"BIGINT", TokenType::BIGINT},
    {"SMALLINT", TokenType::SMALLINT},
    {"FLOAT", TokenType::FLOAT},
    {"DOUBLE", TokenType::DOUBLE},
    {"VARCHAR", TokenType::VARCHAR},
    {"CHAR", TokenType::CHAR},
    {"TEXT", TokenType::TEXT},
    {"BOOLEAN", TokenType::BOOLEAN},
    {"BOOL", TokenType::BOOLEAN},  // Alias

    // Constraints
    {"PRIMARY", TokenType::PRIMARY},
    {"KEY", TokenType::KEY},
    {"FOREIGN", TokenType::FOREIGN},
    {"REFERENCES", TokenType::REFERENCES},
    {"UNIQUE", TokenType::UNIQUE},
    {"NOT", TokenType::NOT},
    {"NULL", TokenType::NULL_LIT},
    {"DEFAULT", TokenType::DEFAULT},

    // Logical operators
    {"AND", TokenType::AND},
    {"OR", TokenType::OR},
    {"TRUE", TokenType::TRUE_LIT},
    {"FALSE", TokenType::FALSE_LIT},
};

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_KEYWORDS_H
