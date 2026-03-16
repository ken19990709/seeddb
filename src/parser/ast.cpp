#include "parser/ast.h"

namespace seeddb {
namespace parser {

std::string data_type_to_string(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::BIGINT: return "BIGINT";
        case DataType::FLOAT: return "FLOAT";
        case DataType::DOUBLE: return "DOUBLE";
        case DataType::VARCHAR: return "VARCHAR";
        case DataType::TEXT: return "TEXT";
        case DataType::BOOLEAN: return "BOOLEAN";
        default: return "UNKNOWN";
    }
}

} // namespace parser
} // namespace seeddb
