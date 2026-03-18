#ifndef SEEDDB_STORAGE_ROW_H
#define SEEDDB_STORAGE_ROW_H

#include <string>
#include <vector>

#include "common/value.h"

namespace seeddb {

// =============================================================================
// Row Class
// =============================================================================

/// Represents a row of values in a table.
/// A simple container for Value objects with type erasure.
/// Each Value carries its own type information.
class Row {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor creates an empty row.
    Row() = default;

    /// Constructor from values.
    /// @param values The vector of values to initialize the row with.
    explicit Row(std::vector<Value> values) : values_(std::move(values)) {}

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Returns the number of values in this row.
    /// @return The number of values.
    size_t size() const { return values_.size(); }

    /// Checks if this row is empty.
    /// @return true if empty, false otherwise.
    bool empty() const { return values_.empty(); }

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Access value at index (const).
    /// @param idx The index of the value.
    /// @return Const reference to the value.
    const Value& get(size_t idx) const { return values_[idx]; }

    /// Access value at index (mutable).
    /// @param idx The index of the value.
    /// @return Mutable reference to the value.
    Value& get(size_t idx) { return values_[idx]; }

    // =========================================================================
    // Modifiers
    // =========================================================================

    /// Append a value to the end of the row.
    /// @param value The value to append.
    void append(Value value) { values_.push_back(std::move(value)); }

    /// Replace value at the specified index.
    /// @param idx The index of the value to replace.
    /// @param value The new value.
    void set(size_t idx, Value value) { values_[idx] = std::move(value); }

    /// Remove all values from the row.
    void clear() { values_.clear(); }

    // =========================================================================
    // Iterator Support
    // =========================================================================

    using iterator = std::vector<Value>::iterator;
    using const_iterator = std::vector<Value>::const_iterator;

    /// Returns an iterator to the beginning.
    iterator begin() { return values_.begin(); }

    /// Returns a const iterator to the beginning.
    const_iterator begin() const { return values_.begin(); }

    /// Returns an iterator to the end.
    iterator end() { return values_.end(); }

    /// Returns a const iterator to the end.
    const_iterator end() const { return values_.end(); }

    // =========================================================================
    // Debug
    // =========================================================================

    /// Returns a string representation of this row.
    /// Format: "(val1, val2, ...)"
    /// @return String representation.
    std::string toString() const {
        std::string result = "(";
        for (size_t i = 0; i < values_.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += values_[i].toString();
        }
        result += ")";
        return result;
    }

private:
    std::vector<Value> values_;  ///< The values in this row.
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_ROW_H
