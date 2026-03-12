#ifndef SEEDDB_PARSER_LEXER_H
#define SEEDDB_PARSER_LEXER_H

#include "parser/token.h"
#include "common/error.h"

#include <string_view>
#include <optional>

namespace seeddb {
namespace parser {

/// SQL lexical analyzer
///
/// NOT thread-safe. Each instance should be used in a single thread.
/// Create separate Lexer instances for concurrent parsing.
class Lexer {
public:
    /// Construct a lexer for the given input
    explicit Lexer(std::string_view input);

    /// Get the next token (consumes it)
    Result<Token> next_token();

    /// Peek at the next token without consuming it
    Result<Token> peek_token();

    /// Check if there are more tokens to read
    bool has_more() const;

    /// Get current source location
    Location current_location() const;

private:
    // Input state
    std::string_view input_;
    size_t position_;
    size_t line_;
    size_t column_;

    // 1-token lookahead buffer
    std::optional<Token> peek_buffer_;

    // Internal scanning methods
    void skip_whitespace_and_comments();
    Result<Token> scan_identifier_or_keyword();
    Result<Token> scan_number();
    Result<Token> scan_string();
    Result<Token> scan_operator();

    // Helper methods
    Token make_token(TokenType type, size_t start_pos, size_t start_line, size_t start_col,
                     TokenValue value = std::monostate{});
    char peek_char(size_t offset = 0) const;
    char advance();
    bool is_at_end() const;
};

} // namespace parser
} // namespace seeddb

#endif // SEEDDB_PARSER_LEXER_H
