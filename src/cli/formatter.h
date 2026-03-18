#ifndef SEEDDB_CLI_FORMATTER_H
#define SEEDDB_CLI_FORMATTER_H

#include <sstream>
#include <string>
#include <vector>

#include "storage/schema.h"
#include "storage/row.h"

namespace seeddb {
namespace cli {

/// Formats query results as ASCII tables (psql-style).
class TableFormatter {
public:
    /// Formats a complete result set as a table.
    /// @param schema The column schema (for headers).
    /// @param rows The result rows.
    /// @return Formatted table string.
    static std::string format(const Schema& schema, const std::vector<Row>& rows);

    /// Formats a single row as a table (for single-row results).
    /// @param schema The column schema.
    /// @param row The row to format.
    /// @return Formatted table string.
    static std::string format(const Schema& schema, const Row& row);

private:
    /// Calculates column widths based on headers and data.
    /// @param schema The column schema.
    /// @param rows The data rows.
    /// @return Vector of column widths.
    static std::vector<size_t> calculateColumnWidths(
        const Schema& schema,
        const std::vector<Row>& rows);

    /// Generates a border line.
    /// @param widths Column widths.
    /// @return Border string like "+---+---+".
    static std::string makeBorder(const std::vector<size_t>& widths);

    /// Generates a header row.
    /// @param schema The column schema.
    /// @param widths Column widths.
    /// @return Header row string.
    static std::string makeHeader(
        const Schema& schema,
        const std::vector<size_t>& widths);

    /// Generates a data row.
    /// @param row The row data.
    /// @param widths Column widths.
    /// @return Data row string.
    static std::string makeRow(
        const Row& row,
        const std::vector<size_t>& widths);

    /// Formats a single cell value.
    /// @param value The value to format.
    /// @param width The column width.
    /// @return Formatted cell string.
    static std::string formatCell(const Value& value, size_t width);

    /// Pads a string to a given width (left-aligned).
    /// @param str The string to pad.
    /// @param width The target width.
    /// @return Padded string.
    static std::string padRight(const std::string& str, size_t width);
};

} // namespace cli
} // namespace seeddb

#endif // SEEDDB_CLI_FORMATTER_H
