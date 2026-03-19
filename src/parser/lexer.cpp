#include "parser/lexer.h"
#include "parser/keywords.h"
#include "common/string_utils.h"

#include <cctype>

namespace seeddb {
namespace parser {

// Token type names for debugging
const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::CREATE: return "CREATE";
        case TokenType::DROP: return "DROP";
        case TokenType::ALTER: return "ALTER";
        case TokenType::TABLE: return "TABLE";
        case TokenType::INDEX: return "INDEX";
        case TokenType::VIEW: return "VIEW";

        case TokenType::SELECT: return "SELECT";
        case TokenType::FROM: return "FROM";
        case TokenType::WHERE: return "WHERE";
        case TokenType::INSERT: return "INSERT";
        case TokenType::INTO: return "INTO";
        case TokenType::UPDATE: return "UPDATE";
        case TokenType::DELETE: return "DELETE";
        case TokenType::VALUES: return "VALUES";
        case TokenType::SET: return "SET";

        case TokenType::JOIN: return "JOIN";
        case TokenType::INNER: return "INNER";
        case TokenType::LEFT: return "LEFT";
        case TokenType::RIGHT: return "RIGHT";
        case TokenType::OUTER: return "OUTER";
        case TokenType::CROSS: return "CROSS";
        case TokenType::ON: return "ON";
        case TokenType::USING: return "USING";

        case TokenType::GROUP: return "GROUP";
        case TokenType::BY: return "BY";
        case TokenType::ORDER: return "ORDER";
        case TokenType::ASC: return "ASC";
        case TokenType::DESC: return "DESC";
        case TokenType::HAVING: return "HAVING";
        case TokenType::LIMIT: return "LIMIT";
        case TokenType::OFFSET: return "OFFSET";
        case TokenType::DISTINCT: return "DISTINCT";

        case TokenType::COUNT: return "COUNT";
        case TokenType::SUM: return "SUM";
        case TokenType::AVG: return "AVG";
        case TokenType::MIN: return "MIN";
        case TokenType::MAX: return "MAX";

        case TokenType::EXISTS: return "EXISTS";
        case TokenType::IN: return "IN";
        case TokenType::BETWEEN: return "BETWEEN";
        case TokenType::LIKE: return "LIKE";
        case TokenType::IS: return "IS";
        case TokenType::AS: return "AS";
        case TokenType::IF: return "IF";

        case TokenType::INTEGER: return "INTEGER";
        case TokenType::BIGINT: return "BIGINT";
        case TokenType::SMALLINT: return "SMALLINT";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::DOUBLE: return "DOUBLE";
        case TokenType::VARCHAR: return "VARCHAR";
        case TokenType::CHAR: return "CHAR";
        case TokenType::TEXT: return "TEXT";
        case TokenType::BOOLEAN: return "BOOLEAN";

        case TokenType::PRIMARY: return "PRIMARY";
        case TokenType::KEY: return "KEY";
        case TokenType::FOREIGN: return "FOREIGN";
        case TokenType::REFERENCES: return "REFERENCES";
        case TokenType::UNIQUE: return "UNIQUE";
        case TokenType::NOT: return "NOT";
        case TokenType::NULL_LIT: return "NULL_LIT";
        case TokenType::DEFAULT: return "DEFAULT";

        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::TRUE_LIT: return "TRUE_LIT";
        case TokenType::FALSE_LIT: return "FALSE_LIT";

        case TokenType::INTEGER_LIT: return "INTEGER_LIT";
        case TokenType::FLOAT_LIT: return "FLOAT_LIT";
        case TokenType::STRING_LIT: return "STRING_LIT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";

        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LE: return "LE";
        case TokenType::GE: return "GE";
        case TokenType::CONCAT: return "CONCAT";

        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";

        case TokenType::END_OF_INPUT: return "END_OF_INPUT";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// Location to_string
std::string Location::to_string() const {
    return std::to_string(line) + ":" + std::to_string(column);
}

// Lexer constructor
Lexer::Lexer(std::string_view input)
    : input_(input)
    , position_(0)
    , line_(1)
    , column_(1)
    , peek_buffer_(std::nullopt)
{}

// Check if at end of input
bool Lexer::is_at_end() const {
    return position_ >= input_.size();
}

// Peek at character at current position + offset
char Lexer::peek_char(size_t offset) const {
    size_t pos = position_ + offset;
    if (pos >= input_.size()) {
        return '\0';
    }
    return input_[pos];
}

// Advance position and return current character
char Lexer::advance() {
    if (is_at_end()) {
        return '\0';
    }
    char c = input_[position_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

// Create a token with location info
Token Lexer::make_token(TokenType type, size_t start_pos, size_t start_line, size_t start_col,
                        TokenValue value) {
    return Token{
        type,
        std::move(value),
        Location{start_line, start_col, start_pos, position_ - start_pos}
    };
}

// Check if there are more tokens
bool Lexer::has_more() const {
    // This is an approximation - actual check requires skipping whitespace
    return position_ < input_.size();
}

// Get current location
Location Lexer::current_location() const {
    return Location{line_, column_, position_, 0};
}

// Skip whitespace and comments
void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek_char();

        // Whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        // Line comment (-- or #)
        if (c == '-' && peek_char(1) == '-') {
            advance();  // first -
            advance();  // second -
            while (!is_at_end() && peek_char() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '#') {
            advance();
            while (!is_at_end() && peek_char() != '\n') {
                advance();
            }
            continue;
        }

        // Block comment (/* */)
        if (c == '/' && peek_char(1) == '*') {
            advance();  // /
            advance();  // *
            while (!is_at_end()) {
                if (peek_char() == '*' && peek_char(1) == '/') {
                    advance();  // *
                    advance();  // /
                    break;
                }
                advance();
            }
            continue;
        }

        // Not whitespace or comment
        break;
    }
}

// Get next token
Result<Token> Lexer::next_token() {
    // Check peek buffer first
    if (peek_buffer_.has_value()) {
        Token tok = std::move(*peek_buffer_);
        peek_buffer_ = std::nullopt;
        return Result<Token>::ok(std::move(tok));
    }

    // Skip whitespace and comments
    skip_whitespace_and_comments();

    // Check for end of input
    if (is_at_end()) {
        size_t pos = position_;
        size_t ln = line_;
        size_t col = column_;
        return Result<Token>::ok(Token{
            TokenType::END_OF_INPUT,
            std::monostate{},
            Location{ln, col, pos, 0}
        });
    }

    // Record start position
    size_t start_pos = position_;
    size_t start_line = line_;
    size_t start_col = column_;

    char c = peek_char();

    // Identifier or keyword
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_identifier_or_keyword();
    }

    // Number
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scan_number();
    }

    // String literal
    if (c == '\'') {
        return scan_string();
    }

    // Two-character operators
    if (c == '<') {
        advance();
        if (peek_char() == '=') {
            advance();
            return Result<Token>::ok(make_token(TokenType::LE, start_pos, start_line, start_col));
        }
        if (peek_char() == '>') {
            advance();
            return Result<Token>::ok(make_token(TokenType::NE, start_pos, start_line, start_col));
        }
        return Result<Token>::ok(make_token(TokenType::LT, start_pos, start_line, start_col));
    }

    if (c == '>') {
        advance();
        if (peek_char() == '=') {
            advance();
            return Result<Token>::ok(make_token(TokenType::GE, start_pos, start_line, start_col));
        }
        return Result<Token>::ok(make_token(TokenType::GT, start_pos, start_line, start_col));
    }

    if (c == '!') {
        advance();
        if (peek_char() == '=') {
            advance();
            return Result<Token>::ok(make_token(TokenType::NE, start_pos, start_line, start_col));
        }
        return Result<Token>::err(
            ErrorCode::UNEXPECTED_CHARACTER,
            "Unexpected character '!' at " + std::to_string(start_line) + ":" + std::to_string(start_col)
        );
    }

    // Concatenation operator ||
    if (c == '|') {
        advance();
        if (peek_char() == '|') {
            advance();
            return Result<Token>::ok(make_token(TokenType::CONCAT, start_pos, start_line, start_col));
        }
        return Result<Token>::err(
            ErrorCode::UNEXPECTED_CHARACTER,
            "Unexpected character '|' at " + std::to_string(start_line) + ":" + std::to_string(start_col)
        );
    }

    // Single-character delimiters and operators
    switch (c) {
        case '(':
            advance();
            return Result<Token>::ok(make_token(TokenType::LPAREN, start_pos, start_line, start_col));
        case ')':
            advance();
            return Result<Token>::ok(make_token(TokenType::RPAREN, start_pos, start_line, start_col));
        case '[':
            advance();
            return Result<Token>::ok(make_token(TokenType::LBRACKET, start_pos, start_line, start_col));
        case ']':
            advance();
            return Result<Token>::ok(make_token(TokenType::RBRACKET, start_pos, start_line, start_col));
        case ',':
            advance();
            return Result<Token>::ok(make_token(TokenType::COMMA, start_pos, start_line, start_col));
        case ';':
            advance();
            return Result<Token>::ok(make_token(TokenType::SEMICOLON, start_pos, start_line, start_col));
        case '.':
            advance();
            return Result<Token>::ok(make_token(TokenType::DOT, start_pos, start_line, start_col));
        case '+':
            advance();
            return Result<Token>::ok(make_token(TokenType::PLUS, start_pos, start_line, start_col));
        case '-':
            advance();
            return Result<Token>::ok(make_token(TokenType::MINUS, start_pos, start_line, start_col));
        case '*':
            advance();
            return Result<Token>::ok(make_token(TokenType::STAR, start_pos, start_line, start_col));
        case '/':
            advance();
            return Result<Token>::ok(make_token(TokenType::SLASH, start_pos, start_line, start_col));
        case '%':
            advance();
            return Result<Token>::ok(make_token(TokenType::PERCENT, start_pos, start_line, start_col));
        case '=':
            advance();
            return Result<Token>::ok(make_token(TokenType::EQ, start_pos, start_line, start_col));
        default:
            advance();
            return Result<Token>::err(
                ErrorCode::UNEXPECTED_CHARACTER,
                "Unexpected character '" + std::string(1, c) + "' at " +
                std::to_string(start_line) + ":" + std::to_string(start_col)
            );
    }
}

// Peek at next token without consuming
Result<Token> Lexer::peek_token() {
    if (!peek_buffer_.has_value()) {
        auto result = next_token();
        if (result.is_ok()) {
            peek_buffer_ = std::move(result.value());
        } else {
            return result;
        }
    }
    return Result<Token>::ok(*peek_buffer_);
}

// Scan identifier or keyword
Result<Token> Lexer::scan_identifier_or_keyword() {
    size_t start_pos = position_;
    size_t start_line = line_;
    size_t start_col = column_;

    // Scan identifier characters
    while (!is_at_end()) {
        char c = peek_char();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    std::string_view text = input_.substr(start_pos, position_ - start_pos);

    // Convert to uppercase for keyword lookup
    std::string upper = utils::to_upper(std::string(text));

    // Check if it's a keyword
    auto it = keywords.find(upper);
    if (it != keywords.end()) {
        return Result<Token>::ok(make_token(it->second, start_pos, start_line, start_col));
    }

    // Regular identifier
    return Result<Token>::ok(make_token(TokenType::IDENTIFIER, start_pos, start_line, start_col,
                                        std::string(text)));
}

// Scan number literal
Result<Token> Lexer::scan_number() {
    size_t start_pos = position_;
    size_t start_line = line_;
    size_t start_col = column_;

    bool is_float = false;

    // Scan integer part
    while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek_char()))) {
        advance();
    }

    // Check for decimal point
    if (peek_char() == '.' && std::isdigit(static_cast<unsigned char>(peek_char(1)))) {
        is_float = true;
        advance();  // consume '.'

        // Scan fractional part
        while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek_char()))) {
            advance();
        }
    }

    // Check for exponent
    if (peek_char() == 'e' || peek_char() == 'E') {
        is_float = true;
        advance();  // consume 'e' or 'E'

        // Optional sign
        if (peek_char() == '+' || peek_char() == '-') {
            advance();
        }

        // Must have at least one digit after exponent
        if (!std::isdigit(static_cast<unsigned char>(peek_char()))) {
            return Result<Token>::err(
                ErrorCode::INVALID_NUMBER,
                "Invalid number format at " + std::to_string(start_line) + ":" + std::to_string(start_col)
            );
        }

        // Scan exponent digits
        while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek_char()))) {
            advance();
        }
    }

    std::string_view text = input_.substr(start_pos, position_ - start_pos);

    if (is_float) {
        try {
            double value = std::stod(std::string(text));
            return Result<Token>::ok(make_token(TokenType::FLOAT_LIT, start_pos, start_line, start_col, value));
        } catch (...) {
            return Result<Token>::err(
                ErrorCode::INVALID_NUMBER,
                "Invalid number format at " + std::to_string(start_line) + ":" + std::to_string(start_col)
            );
        }
    } else {
        try {
            int64_t value = std::stoll(std::string(text));
            return Result<Token>::ok(make_token(TokenType::INTEGER_LIT, start_pos, start_line, start_col, value));
        } catch (...) {
            return Result<Token>::err(
                ErrorCode::INVALID_NUMBER,
                "Invalid number format at " + std::to_string(start_line) + ":" + std::to_string(start_col)
            );
        }
    }
}

// Scan string literal
Result<Token> Lexer::scan_string() {
    size_t start_pos = position_;
    size_t start_line = line_;
    size_t start_col = column_;

    advance();  // consume opening quote

    std::string value;

    while (!is_at_end()) {
        char c = peek_char();

        if (c == '\'') {
            // Check for escaped quote ('')
            if (peek_char(1) == '\'') {
                value += '\'';
                advance();
                advance();
                continue;
            }
            // End of string
            advance();
            return Result<Token>::ok(make_token(TokenType::STRING_LIT, start_pos, start_line, start_col,
                                                std::move(value)));
        }

        value += c;
        advance();
    }

    // Unterminated string
    return Result<Token>::err(
        ErrorCode::UNTERMINATED_STRING,
        "Unterminated string literal at " + std::to_string(start_line) + ":" + std::to_string(start_col)
    );
}

} // namespace parser
} // namespace seeddb
