#include "cli/formatter.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace seeddb {
namespace cli {

std::string TableFormatter::format(const Schema& schema, const std::vector<Row>& rows) {
    if (schema.columnCount() == 0) {
        return "(0 rows)\n";
    }

    std::vector<size_t> widths = calculateColumnWidths(schema, rows);
    std::ostringstream oss;

    // Border line
    oss << makeBorder(widths) << "\n";

    // Header row
    oss << makeHeader(schema, widths) << "\n";

    // Border line
    oss << makeBorder(widths) << "\n";

    // Data rows
    for (const auto& row : rows) {
        oss << makeRow(row, widths) << "\n";
    }

    // Border line (if there are data rows)
    if (!rows.empty()) {
        oss << makeBorder(widths) << "\n";
    }

    // Row count footer
    oss << "(" << rows.size() << " row" << (rows.size() == 1 ? "" : "s") << ")\n";

    return oss.str();
}

std::string TableFormatter::format(const Schema& schema, const Row& row) {
    std::vector<Row> rows;
    if (!row.empty()) {
        rows.push_back(row);
    }
    return format(schema, rows);
}

std::vector<size_t> TableFormatter::calculateColumnWidths(
    const Schema& schema,
    const std::vector<Row>& rows) {

    std::vector<size_t> widths;
    widths.reserve(schema.columnCount());

    // Initialize with header widths
    for (size_t i = 0; i < schema.columnCount(); ++i) {
        widths.push_back(schema.column(i).name().length());
    }

    // Check data widths
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            std::string valStr = row.get(i).toString();
            widths[i] = std::max(widths[i], valStr.length());
        }
    }

    return widths;
}

std::string TableFormatter::makeBorder(const std::vector<size_t>& widths) {
    std::ostringstream oss;
    oss << "+";
    for (size_t w : widths) {
        oss << std::string(w + 2, '-');
        oss << "+";
    }
    return oss.str();
}

std::string TableFormatter::makeHeader(const Schema& schema, const std::vector<size_t>& widths) {
    std::ostringstream oss;
    oss << "|";
    for (size_t i = 0; i < schema.columnCount(); ++i) {
        oss << " " << padRight(schema.column(i).name(), widths[i]) << " |";
    }
    return oss.str();
}

std::string TableFormatter::makeRow(const Row& row, const std::vector<size_t>& widths) {
    std::ostringstream oss;
    oss << "|";
    for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
        oss << " " << formatCell(row.get(i), widths[i]) << " |";
    }
    return oss.str();
}

std::string TableFormatter::formatCell(const Value& value, size_t width) {
    std::string str = value.toString();
    return padRight(str, width);
}

std::string TableFormatter::padRight(const std::string& str, size_t width) {
    if (str.length() >= width) {
        return str;
    }
    return str + std::string(width - str.length(), ' ');
}

} // namespace cli
} // namespace seeddb
