#ifndef SEEDDB_STORAGE_TABLE_H
#define SEEDDB_STORAGE_TABLE_H

#include <string>
#include "storage/schema.h"

namespace seeddb {

// =============================================================================
// Table Class — schema-only metadata container
// =============================================================================
// Row data is stored on disk and accessed via StorageManager::createIterator().
class Table {
public:
    /// Constructs a Table with the given name and schema.
    Table(std::string name, Schema schema)
        : name_(std::move(name)), schema_(std::move(schema)) {}

    /// Returns the table name.
    const std::string& name() const { return name_; }

    /// Returns the table schema.
    const Schema& schema() const { return schema_; }

private:
    std::string name_;
    Schema schema_;
};

}  // namespace seeddb

#endif  // SEEDDB_STORAGE_TABLE_H
