#ifndef SEEDDB_STORAGE_TABLE_H
#define SEEDDB_STORAGE_TABLE_H

#include <string>
#include <vector>

#include "storage/row.h"
#include "storage/schema.h"

namespace seeddb {

// =============================================================================
// Table Class
// =============================================================================

/// Represents a database table with in-memory row storage.
/// Provides basic CRUD operations and iterator support.
class Table {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Constructs a Table with the given name and schema.
    /// @param name The table name.
    /// @param schema The table schema.
    Table(std::string name, Schema schema)
        : name_(std::move(name)), schema_(std::move(schema)) {}

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the table name.
    /// @return Const reference to the table name.
    const std::string& name() const { return name_; }

    /// Returns the table schema.
    /// @return Const reference to the schema.
    const Schema& schema() const { return schema_; }

    /// Returns the number of rows in this table.
    /// @return The row count.
    size_t rowCount() const { return rows_.size(); }

    // =========================================================================
    // Row Operations
    // =========================================================================

    /// Insert a row into the table.
    /// @param row The row to insert.
    void insert(Row row) { rows_.push_back(std::move(row)); }

    /// Get a row by index (const).
    /// @param idx The row index.
    /// @return Const reference to the row.
    const Row& get(size_t idx) const { return rows_[idx]; }

    /// Update a row at the specified index.
    /// @param idx The row index.
    /// @param row The new row value.
    void update(size_t idx, Row row) { rows_[idx] = std::move(row); }

    /// Remove a row at the specified index.
    /// @param idx The row index.
    /// @return true if successful, false if index out of bounds.
    bool remove(size_t idx) {
        if (idx >= rows_.size()) {
            return false;
        }
        rows_.erase(rows_.begin() + static_cast<ptrdiff_t>(idx));
        return true;
    }

    /// Remove multiple rows at the specified indices in O(n) time.
    /// @param indices Sorted vector of indices to remove (ascending order).
    /// @return Number of rows removed.
    /// @note Indices must be sorted in ascending order for correct behavior.
    size_t removeBulk(const std::vector<size_t>& indices) {
        if (indices.empty()) {
            return 0;
        }

        // Build a removal mask
        std::vector<bool> to_remove(rows_.size(), false);
        for (size_t idx : indices) {
            if (idx < rows_.size()) {
                to_remove[idx] = true;
            }
        }

        // Compact: move surviving rows to front (erase-remove idiom)
        size_t write_pos = 0;
        for (size_t read_pos = 0; read_pos < rows_.size(); ++read_pos) {
            if (!to_remove[read_pos]) {
                if (write_pos != read_pos) {
                    rows_[write_pos] = std::move(rows_[read_pos]);
                }
                ++write_pos;
            }
        }

        size_t removed = rows_.size() - write_pos;
        rows_.resize(write_pos);
        return removed;
    }

    /// Remove all rows from the table.
    void clear() { rows_.clear(); }

    // =========================================================================
    // Iterator Support
    // =========================================================================

    using iterator = std::vector<Row>::iterator;
    using const_iterator = std::vector<Row>::const_iterator;

    /// Returns an iterator to the beginning.
    iterator begin() { return rows_.begin(); }

    /// Returns a const iterator to the beginning.
    const_iterator begin() const { return rows_.begin(); }

    /// Returns an iterator to the end.
    iterator end() { return rows_.end(); }

    /// Returns a const iterator to the end.
    const_iterator end() const { return rows_.end(); }

private:
    std::string name_;      ///< Table name.
    Schema schema_;         ///< Table schema.
    std::vector<Row> rows_; ///< In-memory row storage.
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_TABLE_H
