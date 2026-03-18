#ifndef SEEDDB_COMMON_LOGICAL_TYPE_H
#define SEEDDB_COMMON_LOGICAL_TYPE_H

#include <cstddef>

namespace seeddb {

// =============================================================================
// Logical Type ID Enumeration
// =============================================================================

/// Enumeration of SQL logical type identifiers.
/// These represent the runtime type system for database values,
/// distinct from C++ storage types.
enum class LogicalTypeId {
    SQL_NULL = 0,   ///< NULL type
    INTEGER = 1,    ///< 4-byte integer (int32_t)
    BIGINT = 2,     ///< 8-byte integer (int64_t)
    FLOAT = 3,      ///< 4-byte floating point (float)
    DOUBLE = 4,     ///< 8-byte floating point (double)
    VARCHAR = 5,    ///< Variable-length string
    BOOLEAN = 6     ///< Boolean (1 byte)
};

// =============================================================================
// Logical Type Class
// =============================================================================

/// Represents a SQL logical type with type classification utilities.
/// This class wraps a LogicalTypeId and provides convenience methods
/// for type checking and size information.
class LogicalType {
public:
    /// Constructs a LogicalType with the given type ID.
    /// @param id The logical type identifier (defaults to SQL_NULL).
    explicit LogicalType(LogicalTypeId id = LogicalTypeId::SQL_NULL)
        : id_(id) {}

    /// Returns the logical type identifier.
    /// @return The LogicalTypeId of this type.
    LogicalTypeId id() const { return id_; }

    /// Checks if this is a numeric type (INTEGER, BIGINT, FLOAT, DOUBLE).
    /// @return true if numeric, false otherwise.
    bool isNumeric() const {
        return id_ == LogicalTypeId::INTEGER
            || id_ == LogicalTypeId::BIGINT
            || id_ == LogicalTypeId::FLOAT
            || id_ == LogicalTypeId::DOUBLE;
    }

    /// Checks if this is an integer type (INTEGER, BIGINT).
    /// @return true if integer, false otherwise.
    bool isInteger() const {
        return id_ == LogicalTypeId::INTEGER
            || id_ == LogicalTypeId::BIGINT;
    }

    /// Checks if this is a floating-point type (FLOAT, DOUBLE).
    /// @return true if floating-point, false otherwise.
    bool isFloating() const {
        return id_ == LogicalTypeId::FLOAT
            || id_ == LogicalTypeId::DOUBLE;
    }

    /// Checks if this is a string type (VARCHAR).
    /// @return true if string, false otherwise.
    bool isString() const {
        return id_ == LogicalTypeId::VARCHAR;
    }

    /// Returns the fixed byte size for fixed-size types.
    /// Returns 0 for variable-length types (VARCHAR, SQL_NULL).
    /// @return Size in bytes, or 0 for variable-length.
    size_t fixedSize() const {
        switch (id_) {
            case LogicalTypeId::INTEGER:
                return 4;  // int32_t
            case LogicalTypeId::BIGINT:
                return 8;  // int64_t
            case LogicalTypeId::FLOAT:
                return 4;  // float
            case LogicalTypeId::DOUBLE:
                return 8;  // double
            case LogicalTypeId::BOOLEAN:
                return 1;  // bool (1 byte)
            case LogicalTypeId::SQL_NULL:
            case LogicalTypeId::VARCHAR:
            default:
                return 0;  // variable-length
        }
    }

private:
    LogicalTypeId id_;
};

} // namespace seeddb

#endif // SEEDDB_COMMON_LOGICAL_TYPE_H
