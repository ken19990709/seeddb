#ifndef SEEDDB_COMMON_ERROR_H
#define SEEDDB_COMMON_ERROR_H

#include <string>
#include <stdexcept>
#include <optional>

namespace seeddb {

/// Error codes for the database system
enum class ErrorCode : int {
    SUCCESS = 0,

    // General errors (1-99)
    UNKNOWN_ERROR = 1,
    INTERNAL_ERROR = 2,
    NOT_IMPLEMENTED = 3,

    // SQL parsing errors (100-199)
    SYNTAX_ERROR = 100,
    INVALID_IDENTIFIER = 101,

    // Lexer errors (110-119)
    UNEXPECTED_CHARACTER = 110,
    UNTERMINATED_STRING = 111,
    INVALID_NUMBER = 112,
    INVALID_ESCAPE_SEQUENCE = 113,

    // Semantic errors (200-299)
    TABLE_NOT_FOUND = 200,
    COLUMN_NOT_FOUND = 201,
    INDEX_NOT_FOUND = 202,
    DUPLICATE_TABLE = 203,
    DUPLICATE_COLUMN = 204,

    // Type errors (300-399)
    TYPE_ERROR = 300,
    TYPE_MISMATCH = 301,
    INVALID_CAST = 302,

    // Constraint errors (400-499)
    CONSTRAINT_VIOLATION = 400,
    NOT_NULL_VIOLATION = 401,
    UNIQUE_VIOLATION = 402,
    FOREIGN_KEY_VIOLATION = 403,

    // Transaction errors (500-599)
    TRANSACTION_ERROR = 500,
    DEADLOCK_DETECTED = 501,
    SERIALIZATION_FAILURE = 502,

    // I/O errors (600-699)
    IO_ERROR = 600,
    FILE_NOT_FOUND = 601,
    DISK_FULL = 602,

    // Connection errors (700-799)
    CONNECTION_ERROR = 700,
    PROTOCOL_ERROR = 701,
};

/// Get human-readable name for error code
const char* error_code_name(ErrorCode code);

/// Get default message for error code
const char* error_code_message(ErrorCode code);

/// Error class - represents a database error
class Error : public std::exception {
public:
    Error(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {
        format_what();
    }

    Error(ErrorCode code)
        : code_(code), message_(error_code_message(code)) {
        format_what();
    }

    ErrorCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }
    bool ok() const noexcept { return code_ == ErrorCode::SUCCESS; }

    const char* what() const noexcept override { return what_.c_str(); }

private:
    void format_what() {
        what_ = std::string("[") + error_code_name(code_) + "] " + message_;
    }

    ErrorCode code_;
    std::string message_;
    std::string what_;
};

/// Result<T> - either a value or an error
template<typename T>
class Result {
public:
    /// Create a successful result with a value
    static Result ok(T value) {
        return Result(std::move(value), std::nullopt);
    }

    /// Create a failed result with an error
    static Result err(ErrorCode code, std::string message) {
        return Result(std::nullopt, Error(code, std::move(message)));
    }

    /// Create a failed result from an Error
    static Result err(Error error) {
        return Result(std::nullopt, std::move(error));
    }

    /// Check if result is successful
    bool is_ok() const noexcept {
        return !error_.has_value() || error_->code() == ErrorCode::SUCCESS;
    }

    /// Check if result is successful (alias for is_ok)
    bool ok() const noexcept {
        return is_ok();
    }

    /// Get the value (undefined behavior if not ok())
    T& value() & {
        return *value_;
    }

    /// Get the value (undefined behavior if not ok())
    const T& value() const& {
        return *value_;
    }

    /// Get the value (undefined behavior if not ok())
    T&& value() && {
        return std::move(*value_);
    }

    /// Get the error (undefined behavior if ok())
    const Error& error() const noexcept {
        return *error_;
    }

private:
    Result(std::optional<T> value, std::optional<Error> error)
        : value_(std::move(value)), error_(std::move(error)) {}

    std::optional<T> value_;
    std::optional<Error> error_;
};

/// Specialization for void (no value, just success/failure)
template<>
class Result<void> {
public:
    static Result ok() { return Result(std::nullopt); }
    static Result err(ErrorCode code, std::string message) {
        return Result(Error(code, std::move(message)));
    }
    static Result err(Error error) {
        return Result(std::move(error));
    }

    /// Check if result is successful
    /// Note: Use is_ok() instead of ok() to avoid conflict with static factory
    bool is_ok() const noexcept {
        return !error_.has_value() || error_->code() == ErrorCode::SUCCESS;
    }

    const Error& error() const noexcept { return *error_; }

private:
    explicit Result(std::optional<Error> error = std::nullopt)
        : error_(std::move(error)) {}

    std::optional<Error> error_;
};

} // namespace seeddb

#endif // SEEDDB_COMMON_ERROR_H
