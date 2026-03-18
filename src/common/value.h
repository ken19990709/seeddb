#ifndef SEEDDB_COMMON_VALUE_H
#define SEEDDB_COMMON_VALUE_H

#include <cstdint>
#include <limits>
#include <string>

#include "common/logical_type.h"

namespace seeddb {

// =============================================================================
// Value Class
// =============================================================================

/// Represents a runtime SQL value with its logical type.
/// Uses a union for fixed-size types and a separate std::string for VARCHAR.
class Value {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor creates a NULL value.
    Value() : type_(LogicalTypeId::SQL_NULL), str_val_() {}

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /// Creates a NULL value.
    /// @return A Value representing NULL.
    static Value null() {
        return Value();
    }

    /// Creates an INTEGER value.
    /// @param v The 32-bit integer value.
    /// @return A Value of type INTEGER.
    static Value integer(int32_t v) {
        Value val;
        val.type_ = LogicalType(LogicalTypeId::INTEGER);
        val.data_.int32_val = v;
        return val;
    }

    /// Creates a BIGINT value.
    /// @param v The 64-bit integer value.
    /// @return A Value of type BIGINT.
    static Value bigint(int64_t v) {
        Value val;
        val.type_ = LogicalType(LogicalTypeId::BIGINT);
        val.data_.int64_val = v;
        return val;
    }

    /// Creates a FLOAT value.
    /// @param v The float value.
    /// @return A Value of type FLOAT.
    static Value Float(float v) {
        Value val;
        val.type_ = LogicalType(LogicalTypeId::FLOAT);
        val.data_.float_val = v;
        return val;
    }

    /// Creates a DOUBLE value.
    /// @param v The double value.
    /// @return A Value of type DOUBLE.
    static Value Double(double v) {
        Value val;
        val.type_ = LogicalType(LogicalTypeId::DOUBLE);
        val.data_.double_val = v;
        return val;
    }

    /// Creates a VARCHAR value.
    /// @param v The string value.
    /// @return A Value of type VARCHAR.
    static Value varchar(std::string v) {
        Value val;
        val.type_ = LogicalType(LogicalTypeId::VARCHAR);
        val.str_val_ = std::move(v);
        return val;
    }

    /// Creates a BOOLEAN value.
    /// @param v The boolean value.
    /// @return A Value of type BOOLEAN.
    static Value boolean(bool v) {
        Value val;
        val.type_ = LogicalType(LogicalTypeId::BOOLEAN);
        val.data_.bool_val = v;
        return val;
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the logical type of this value.
    /// @return Reference to the LogicalType.
    const LogicalType& type() const { return type_; }

    /// Returns the logical type ID of this value.
    /// @return The LogicalTypeId.
    LogicalTypeId typeId() const { return type_.id(); }

    /// Checks if this value is NULL.
    /// @return true if NULL, false otherwise.
    bool isNull() const { return type_.id() == LogicalTypeId::SQL_NULL; }

    /// Returns the value as a 32-bit integer.
    /// Precondition: typeId() == LogicalTypeId::INTEGER
    /// @return The integer value.
    int32_t asInt32() const { return data_.int32_val; }

    /// Returns the value as a 64-bit integer.
    /// Precondition: typeId() == LogicalTypeId::BIGINT
    /// @return The 64-bit integer value.
    int64_t asInt64() const { return data_.int64_val; }

    /// Returns the value as a float.
    /// Precondition: typeId() == LogicalTypeId::FLOAT
    /// @return The float value.
    float asFloat() const { return data_.float_val; }

    /// Returns the value as a double.
    /// Precondition: typeId() == LogicalTypeId::DOUBLE
    /// @return The double value.
    double asDouble() const { return data_.double_val; }

    /// Returns the value as a string reference.
    /// Precondition: typeId() == LogicalTypeId::VARCHAR
    /// @return Reference to the string value.
    const std::string& asString() const { return str_val_; }

    /// Returns the value as a boolean.
    /// Precondition: typeId() == LogicalTypeId::BOOLEAN
    /// @return The boolean value.
    bool asBool() const { return data_.bool_val; }

    // =========================================================================
    // Comparison
    // =========================================================================

    /// Compares this value with another for equality.
    /// NULL equals NULL. Different types return false.
    /// @param other The value to compare with.
    /// @return true if equal, false otherwise.
    bool equals(const Value& other) const {
        // NULL equals NULL
        if (isNull() && other.isNull()) {
            return true;
        }

        // NULL != non-NULL
        if (isNull() || other.isNull()) {
            return false;
        }

        // Different types are not equal
        if (typeId() != other.typeId()) {
            return false;
        }

        // Same type comparison
        switch (typeId()) {
            case LogicalTypeId::INTEGER:
                return data_.int32_val == other.data_.int32_val;
            case LogicalTypeId::BIGINT:
                return data_.int64_val == other.data_.int64_val;
            case LogicalTypeId::FLOAT:
                return data_.float_val == other.data_.float_val;
            case LogicalTypeId::DOUBLE:
                return data_.double_val == other.data_.double_val;
            case LogicalTypeId::VARCHAR:
                return str_val_ == other.str_val_;
            case LogicalTypeId::BOOLEAN:
                return data_.bool_val == other.data_.bool_val;
            default:
                return false;
        }
    }

    /// Compares this value with another for less-than ordering.
    /// NULL < non-NULL. Different types use type ID ordering.
    /// @param other The value to compare with.
    /// @return true if this < other, false otherwise.
    bool lessThan(const Value& other) const {
        // NULL < non-NULL
        if (isNull() && !other.isNull()) {
            return true;
        }

        // non-NULL is not less than NULL
        if (!isNull() && other.isNull()) {
            return false;
        }

        // Two NULLs are not less than each other
        if (isNull() && other.isNull()) {
            return false;
        }

        // Different types: use type ID ordering
        if (typeId() != other.typeId()) {
            return static_cast<int>(typeId()) < static_cast<int>(other.typeId());
        }

        // Same type comparison
        switch (typeId()) {
            case LogicalTypeId::INTEGER:
                return data_.int32_val < other.data_.int32_val;
            case LogicalTypeId::BIGINT:
                return data_.int64_val < other.data_.int64_val;
            case LogicalTypeId::FLOAT:
                return data_.float_val < other.data_.float_val;
            case LogicalTypeId::DOUBLE:
                return data_.double_val < other.data_.double_val;
            case LogicalTypeId::VARCHAR:
                return str_val_ < other.str_val_;
            case LogicalTypeId::BOOLEAN:
                // false < true (0 < 1)
                return data_.bool_val < other.data_.bool_val;
            default:
                return false;
        }
    }

    // =========================================================================
    // Debug
    // =========================================================================

    /// Returns a string representation of this value.
    /// @return "NULL" for null, "true"/"false" for bool, etc.
    std::string toString() const {
        if (isNull()) {
            return "NULL";
        }

        switch (typeId()) {
            case LogicalTypeId::INTEGER:
                return std::to_string(data_.int32_val);
            case LogicalTypeId::BIGINT:
                return std::to_string(data_.int64_val);
            case LogicalTypeId::FLOAT:
                return std::to_string(data_.float_val);
            case LogicalTypeId::DOUBLE:
                return std::to_string(data_.double_val);
            case LogicalTypeId::VARCHAR:
                return str_val_;
            case LogicalTypeId::BOOLEAN:
                return data_.bool_val ? "true" : "false";
            default:
                return "UNKNOWN";
        }
    }

private:
    /// Union for fixed-size type storage.
    union Data {
        int32_t int32_val;
        int64_t int64_val;
        float float_val;
        double double_val;
        bool bool_val;

        Data() : int64_val(0) {}  // Default initialize to 0
    };

    LogicalType type_;   ///< The logical type of this value.
    Data data_;          ///< Storage for fixed-size types.
    std::string str_val_; ///< Storage for VARCHAR (separate from union).
};

} // namespace seeddb

#endif // SEEDDB_COMMON_VALUE_H
