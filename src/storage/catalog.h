#ifndef SEEDDB_STORAGE_CATALOG_H
#define SEEDDB_STORAGE_CATALOG_H

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "storage/schema.h"
#include "storage/table.h"

namespace seeddb {

// =============================================================================
// Catalog Class
// =============================================================================

/// Manages all tables in the database.
/// Uses unique_ptr for pointer stability when the map rehashes.
class Catalog {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor creates an empty catalog.
    Catalog() = default;

    // =========================================================================
    // Table Management
    // =========================================================================

    /// Create a new table with the given name and schema.
    /// @param name The table name.
    /// @param schema The table schema.
    /// @return true if created, false if table already exists.
    bool createTable(std::string name, Schema schema) {
        if (tables_.find(name) != tables_.end()) {
            return false;
        }
        auto table = std::make_unique<Table>(name, std::move(schema));
        tables_.emplace(std::move(name), std::move(table));
        return true;
    }

    /// Drop a table by name.
    /// @param name The table name.
    /// @return true if dropped, false if table does not exist.
    bool dropTable(const std::string& name) {
        auto it = tables_.find(name);
        if (it == tables_.end()) {
            return false;
        }
        tables_.erase(it);
        return true;
    }

    /// Check if a table exists.
    /// @param name The table name.
    /// @return true if table exists, false otherwise.
    bool hasTable(const std::string& name) const {
        return tables_.find(name) != tables_.end();
    }

    /// Get a mutable pointer to a table.
    /// @param name The table name.
    /// @return Pointer to the table, or nullptr if not found.
    Table* getTable(const std::string& name) {
        auto it = tables_.find(name);
        return it != tables_.end() ? it->second.get() : nullptr;
    }

    /// Get a const pointer to a table.
    /// @param name The table name.
    /// @return Const pointer to the table, or nullptr if not found.
    const Table* getTable(const std::string& name) const {
        auto it = tables_.find(name);
        return it != tables_.end() ? it->second.get() : nullptr;
    }

    /// Returns the number of tables in the catalog.
    /// @return The table count.
    size_t tableCount() const { return tables_.size(); }

    // =========================================================================
    // Iterator Support
    // =========================================================================

    /// Iterator that yields std::pair<const std::string&, Table*>
    class Iterator {
    public:
        using MapIterator = std::unordered_map<std::string, std::unique_ptr<Table>>::iterator;

        explicit Iterator(MapIterator it) : it_(it) {}

        std::pair<const std::string&, Table*> operator*() {
            return {it_->first, it_->second.get()};
        }

        Iterator& operator++() {
            ++it_;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return it_ != other.it_;
        }

    private:
        MapIterator it_;
    };

    /// Const iterator that yields std::pair<const std::string&, const Table*>
    class ConstIterator {
    public:
        using MapConstIterator = std::unordered_map<std::string, std::unique_ptr<Table>>::const_iterator;

        explicit ConstIterator(MapConstIterator it) : it_(it) {}

        std::pair<const std::string&, const Table*> operator*() const {
            return {it_->first, it_->second.get()};
        }

        ConstIterator& operator++() {
            ++it_;
            return *this;
        }

        bool operator!=(const ConstIterator& other) const {
            return it_ != other.it_;
        }

    private:
        MapConstIterator it_;
    };

    using iterator = Iterator;
    using const_iterator = ConstIterator;

    /// Returns an iterator to the beginning.
    iterator begin() { return Iterator(tables_.begin()); }

    /// Returns a const iterator to the beginning.
    const_iterator begin() const { return ConstIterator(tables_.begin()); }

    /// Returns an iterator to the end.
    iterator end() { return Iterator(tables_.end()); }

    /// Returns a const iterator to the end.
    const_iterator end() const { return ConstIterator(tables_.end()); }

private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
};

} // namespace seeddb

#endif // SEEDDB_STORAGE_CATALOG_H
