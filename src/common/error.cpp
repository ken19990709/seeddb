#include "common/error.h"

namespace seeddb {

const char* error_code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS:           return "SUCCESS";

        // General errors
        case ErrorCode::UNKNOWN_ERROR:     return "UNKNOWN_ERROR";
        case ErrorCode::INTERNAL_ERROR:    return "INTERNAL_ERROR";
        case ErrorCode::NOT_IMPLEMENTED:   return "NOT_IMPLEMENTED";

        // Parsing errors
        case ErrorCode::SYNTAX_ERROR:      return "SYNTAX_ERROR";
        case ErrorCode::INVALID_IDENTIFIER: return "INVALID_IDENTIFIER";

        // Lexer errors
        case ErrorCode::UNEXPECTED_CHARACTER: return "UNEXPECTED_CHARACTER";
        case ErrorCode::UNTERMINATED_STRING: return "UNTERMINATED_STRING";
        case ErrorCode::INVALID_NUMBER:    return "INVALID_NUMBER";
        case ErrorCode::INVALID_ESCAPE_SEQUENCE: return "INVALID_ESCAPE_SEQUENCE";

        // Semantic errors
        case ErrorCode::TABLE_NOT_FOUND:   return "TABLE_NOT_FOUND";
        case ErrorCode::COLUMN_NOT_FOUND:  return "COLUMN_NOT_FOUND";
        case ErrorCode::INDEX_NOT_FOUND:   return "INDEX_NOT_FOUND";
        case ErrorCode::DUPLICATE_TABLE:   return "DUPLICATE_TABLE";
        case ErrorCode::DUPLICATE_COLUMN:  return "DUPLICATE_COLUMN";

        // Type errors
        case ErrorCode::TYPE_ERROR:        return "TYPE_ERROR";
        case ErrorCode::TYPE_MISMATCH:     return "TYPE_MISMATCH";
        case ErrorCode::INVALID_CAST:      return "INVALID_CAST";

        // Constraint errors
        case ErrorCode::CONSTRAINT_VIOLATION: return "CONSTRAINT_VIOLATION";
        case ErrorCode::NOT_NULL_VIOLATION: return "NOT_NULL_VIOLATION";
        case ErrorCode::UNIQUE_VIOLATION:  return "UNIQUE_VIOLATION";
        case ErrorCode::FOREIGN_KEY_VIOLATION: return "FOREIGN_KEY_VIOLATION";

        // Transaction errors
        case ErrorCode::TRANSACTION_ERROR: return "TRANSACTION_ERROR";
        case ErrorCode::DEADLOCK_DETECTED: return "DEADLOCK_DETECTED";
        case ErrorCode::SERIALIZATION_FAILURE: return "SERIALIZATION_FAILURE";

        // I/O errors
        case ErrorCode::IO_ERROR:          return "IO_ERROR";
        case ErrorCode::FILE_NOT_FOUND:    return "FILE_NOT_FOUND";
        case ErrorCode::DISK_FULL:         return "DISK_FULL";

        // Connection errors
        case ErrorCode::CONNECTION_ERROR:  return "CONNECTION_ERROR";
        case ErrorCode::PROTOCOL_ERROR:    return "PROTOCOL_ERROR";

        default:                           return "UNKNOWN";
    }
}

const char* error_code_message(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS:           return "Success";

        // General errors
        case ErrorCode::UNKNOWN_ERROR:     return "An unknown error occurred";
        case ErrorCode::INTERNAL_ERROR:    return "Internal error";
        case ErrorCode::NOT_IMPLEMENTED:   return "Feature not implemented";

        // Parsing errors
        case ErrorCode::SYNTAX_ERROR:      return "Syntax error";
        case ErrorCode::INVALID_IDENTIFIER: return "Invalid identifier";

        // Lexer errors
        case ErrorCode::UNEXPECTED_CHARACTER: return "Unexpected character";
        case ErrorCode::UNTERMINATED_STRING: return "Unterminated string literal";
        case ErrorCode::INVALID_NUMBER:    return "Invalid number format";
        case ErrorCode::INVALID_ESCAPE_SEQUENCE: return "Invalid escape sequence";

        // Semantic errors
        case ErrorCode::TABLE_NOT_FOUND:   return "Table not found";
        case ErrorCode::COLUMN_NOT_FOUND:  return "Column not found";
        case ErrorCode::INDEX_NOT_FOUND:   return "Index not found";
        case ErrorCode::DUPLICATE_TABLE:   return "Table already exists";
        case ErrorCode::DUPLICATE_COLUMN:  return "Column already exists";

        // Type errors
        case ErrorCode::TYPE_ERROR:        return "Type error";
        case ErrorCode::TYPE_MISMATCH:     return "Type mismatch";
        case ErrorCode::INVALID_CAST:      return "Invalid type cast";

        // Constraint errors
        case ErrorCode::CONSTRAINT_VIOLATION: return "Constraint violation";
        case ErrorCode::NOT_NULL_VIOLATION: return "Null value violates not-null constraint";
        case ErrorCode::UNIQUE_VIOLATION:  return "Duplicate key value violates unique constraint";
        case ErrorCode::FOREIGN_KEY_VIOLATION: return "Foreign key constraint violation";

        // Transaction errors
        case ErrorCode::TRANSACTION_ERROR: return "Transaction error";
        case ErrorCode::DEADLOCK_DETECTED: return "Deadlock detected";
        case ErrorCode::SERIALIZATION_FAILURE: return "Serialization failure";

        // I/O errors
        case ErrorCode::IO_ERROR:          return "I/O error";
        case ErrorCode::FILE_NOT_FOUND:    return "File not found";
        case ErrorCode::DISK_FULL:         return "Disk is full";

        // Connection errors
        case ErrorCode::CONNECTION_ERROR:  return "Connection error";
        case ErrorCode::PROTOCOL_ERROR:    return "Protocol error";

        default:                           return "Unknown error";
    }
}

} // namespace seeddb
