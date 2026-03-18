#ifndef SEEDDB_STORAGE_SCHEMA_H
#define SEEDDB_STORAGE_SCHEMA_H

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/logical_type.h"
#include "storage/row.h"

namespace seeddb {

// =============================================================================
// Helper Functions
// =============================================================================

/// Returns the string name for a logical type ID.
/// @param id The logical type identifier.
/// @return String representation of the type.
inline const char* logical_type_name(LogicalTypeId id) {
    switch (id) {
        case LogicalTypeId::SQL_NULL:
            return "SQL_NULL";
        case LogicalTypeId::INTEGER:
            return "INTEGER";
        case LogicalTypeId::BIGINT:
            return "BIGINT";
        case LogicalTypeId::FLOAT:
            return "FLOAT";
        case LogicalTypeId::DOUBLE:
            return "DOUBLE";
        case LogicalTypeId::VARCHAR:
            return "VARCHAR";
        case LogicalTypeId::BOOLEAN:
            return "BOOLEAN";
        default:
            return "UNKNOWN";
    }
}

// =============================================================================
// ColumnSchema Class
// =============================================================================

/// Represents the schema of a single column in a table.
/// Contains the column name, logical type, and nullability.
class ColumnSchema {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Constructs a ColumnSchema with the given name, type, and nullability.
    /// @param name The column name.
    /// @param type The logical type of the column.
    /// @param nullable Whether the column allows NULL values (default true).
    ColumnSchema(std::string name, LogicalType type, bool nullable = true)
        : name_(std::move(name)), type_(type), nullable_(nullable) {}

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the column name.
    /// @return Reference to the column name string.
    const std::string& name() const { return name_; }

    /// Returns the logical type of this column.
    /// @return Reference to the LogicalType.
    const LogicalType& type() const { return type_; }

    /// Checks if this column allows NULL values.
    /// @return true if nullable, false otherwise.
    bool isNullable() const { return nullable_; }

private:
    std::string name_;   ///< Column name.
    LogicalType type_;   ///< Column logical type.
    bool nullable_;      ///< Whether NULL is allowed.
};

// =============================================================================
// Schema Class
// =============================================================================

/// Represents the schema of a table (collection of columns).
/// Provides column lookup by index or name, and row validation.
class Schema {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor creates an empty schema.
    Schema() = default;

    /// Constructs a Schema from a vector of column definitions.
    /// @param columns The column definitions.
    explicit Schema(std::vector<ColumnSchema> columns) : columns_(std::move(columns)) {
        // Build name -> index mapping
        for (size_t i = 0; i < columns_.size(); ++i) {
            name_to_index_[columns_[i].name()] = i;
        }
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Returns the number of columns in this schema.
    /// @return The number of columns.
    size_t columnCount() const { return columns_.size(); }

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the column at the specified index.
    /// Precondition: idx < columnCount()
    /// @param idx The column index.
    /// @return Const reference to the ColumnSchema.
    const ColumnSchema& column(size_t idx) const { return columns_[idx]; }

    /// Returns the column with the specified name.
    /// Throws std::out_of_range if not found.
    /// @param name The column name.
    /// @return Const reference to the ColumnSchema.
    const ColumnSchema& column(const std::string& name) const {
        auto it = name_to_index_.find(name);
        if (it == name_to_index_.end()) {
            throw std::out_of_range("Column not found: " + name);
        }
        return columns_[it->second];
    }

    /// Checks if a column with the given name exists.
    /// @param name The column name to search for.
    /// @return true if column exists, false otherwise.
    bool hasColumn(const std::string& name) const {
        return name_to_index_.find(name) != name_to_index_.end();
    }

    /// Returns the index of the column with the given name.
    /// @param name The column name to search for.
    /// @return The column index, or nullopt if not found.
    std::optional<size_t> columnIndex(const std::string& name) const {
        auto it = name_to_index_.find(name);
        if (it == name_to_index_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // =========================================================================
    // Validation
    // =========================================================================

    /// Validates a row against this schema.
    /// Checks that:
    /// 1. The row has the correct number of columns
    /// 2. NULL values are not in non-nullable columns
    /// @param row The row to validate.
    /// @return true if the row is valid, false otherwise.
    bool validateRow(const Row& row) const {
        // Check column count
        if (row.size() != columns_.size()) {
            return false;
        }

        // Check NOT NULL constraints
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (!columns_[i].isNullable() && row.get(i).isNull()) {
                return false;
            }
        }

        return true;
    }

    // =========================================================================
    // Debug
    // =========================================================================

    /// Returns a string representation of this schema.
    /// Format: "name TYPE [NOT NULL], ..."
    /// @return String representation.
    std::string toString() const {
        std::string result;
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += columns_[i].name();
            result += " ";
            result += logical_type_name(columns_[i].type().id());
            if (!columns_[i].isNullable()) {
                result += " NOT NULL";
            }
        }
        return result;
    }

private:
    std::vector<ColumnSchema> columns_;              ///< Column definitions.
    std::unordered_map<std::string, size_t> name_to_index_;  ///< Name -> index mapping.
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_SCHEMA_H
