#ifndef SEEDDB_STORAGE_ROW_SERIALIZER_H
#define SEEDDB_STORAGE_ROW_SERIALIZER_H

#include <cstring>
#include <string>
#include <vector>

#include "common/logical_type.h"
#include "common/value.h"
#include "storage/row.h"
#include "storage/schema.h"

namespace seeddb {

// =============================================================================
// RowSerializer — schema-driven binary Row serialization
// =============================================================================
//
// Binary format per column (in schema column order):
//   [1 byte: null flag]  — 0x01 = NULL, 0x00 = not null
//   If not null:
//     INTEGER  → 4 bytes LE int32_t
//     BIGINT   → 8 bytes LE int64_t
//     FLOAT    → 4 bytes IEEE-754 float
//     DOUBLE   → 8 bytes IEEE-754 double
//     BOOLEAN  → 1 byte  (0x00 = false, 0x01 = true)
//     VARCHAR  → 4 bytes LE uint32_t length + N bytes UTF-8 string
//
// The schema is required for both serialization and deserialization;
// no type information is stored in the byte stream.
// =============================================================================

class RowSerializer {
public:
    // =========================================================================
    // Serialize
    // =========================================================================

    /// Serializes a Row to a byte vector using the provided schema.
    /// @param row    The row to serialize.
    /// @param schema The table schema (defines column types and count).
    /// @return A heap-allocated byte vector.
    static std::vector<char> serialize(const Row& row, const Schema& schema) {
        // --- First pass: compute required buffer size ---
        size_t total = 0;
        for (size_t i = 0; i < schema.columnCount(); ++i) {
            total += 1;  // null flag
            const Value& val = row.get(i);
            if (!val.isNull()) {
                switch (schema.column(i).type().id()) {
                    case LogicalTypeId::INTEGER:  total += 4; break;
                    case LogicalTypeId::BIGINT:   total += 8; break;
                    case LogicalTypeId::FLOAT:    total += 4; break;
                    case LogicalTypeId::DOUBLE:   total += 8; break;
                    case LogicalTypeId::BOOLEAN:  total += 1; break;
                    case LogicalTypeId::VARCHAR:  total += 4 + val.asString().size(); break;
                    default: break;
                }
            }
        }

        // --- Second pass: write bytes ---
        std::vector<char> buf(total);
        char* ptr = buf.data();

        for (size_t i = 0; i < schema.columnCount(); ++i) {
            const Value& val = row.get(i);
            if (val.isNull()) {
                *ptr++ = static_cast<char>(0x01);  // null
                continue;
            }
            *ptr++ = static_cast<char>(0x00);  // not null

            switch (schema.column(i).type().id()) {
                case LogicalTypeId::INTEGER: {
                    int32_t v = val.asInt32();
                    std::memcpy(ptr, &v, 4); ptr += 4;
                    break;
                }
                case LogicalTypeId::BIGINT: {
                    int64_t v = val.asInt64();
                    std::memcpy(ptr, &v, 8); ptr += 8;
                    break;
                }
                case LogicalTypeId::FLOAT: {
                    float v = val.asFloat();
                    std::memcpy(ptr, &v, 4); ptr += 4;
                    break;
                }
                case LogicalTypeId::DOUBLE: {
                    double v = val.asDouble();
                    std::memcpy(ptr, &v, 8); ptr += 8;
                    break;
                }
                case LogicalTypeId::BOOLEAN: {
                    uint8_t v = val.asBool() ? 0x01 : 0x00;
                    std::memcpy(ptr, &v, 1); ptr += 1;
                    break;
                }
                case LogicalTypeId::VARCHAR: {
                    const std::string& s = val.asString();
                    uint32_t len = static_cast<uint32_t>(s.size());
                    std::memcpy(ptr, &len, 4); ptr += 4;
                    std::memcpy(ptr, s.data(), len); ptr += len;
                    break;
                }
                default:
                    break;
            }
        }

        return buf;
    }

    // =========================================================================
    // Deserialize
    // =========================================================================

    /// Deserializes a Row from a byte buffer using the provided schema.
    /// Extra bytes beyond the row encoding are silently ignored.
    /// @param data   Pointer to the serialized bytes.
    /// @param size   Number of bytes available.
    /// @param schema The table schema (defines column types and count).
    /// @return The deserialized Row.
    static Row deserialize(const char* data, size_t size, const Schema& schema) {
        const char* ptr = data;
        const char* end = data + size;

        std::vector<Value> values;
        values.reserve(schema.columnCount());

        for (size_t i = 0; i < schema.columnCount(); ++i) {
            // Check bounds for null flag
            if (ptr >= end) {
                values.push_back(Value::null());
                continue;
            }

            uint8_t is_null = static_cast<uint8_t>(*ptr++);
            if (is_null) {
                values.push_back(Value::null());
                continue;
            }

            switch (schema.column(i).type().id()) {
                case LogicalTypeId::INTEGER: {
                    if (ptr + 4 > end) {
                        values.push_back(Value::null());
                        ptr = end;  // Skip remaining
                        continue;
                    }
                    int32_t v = 0;
                    std::memcpy(&v, ptr, 4); ptr += 4;
                    values.push_back(Value::integer(v));
                    break;
                }
                case LogicalTypeId::BIGINT: {
                    if (ptr + 8 > end) {
                        values.push_back(Value::null());
                        ptr = end;
                        continue;
                    }
                    int64_t v = 0;
                    std::memcpy(&v, ptr, 8); ptr += 8;
                    values.push_back(Value::bigint(v));
                    break;
                }
                case LogicalTypeId::FLOAT: {
                    if (ptr + 4 > end) {
                        values.push_back(Value::null());
                        ptr = end;
                        continue;
                    }
                    float v = 0.0f;
                    std::memcpy(&v, ptr, 4); ptr += 4;
                    values.push_back(Value::Float(v));
                    break;
                }
                case LogicalTypeId::DOUBLE: {
                    if (ptr + 8 > end) {
                        values.push_back(Value::null());
                        ptr = end;
                        continue;
                    }
                    double v = 0.0;
                    std::memcpy(&v, ptr, 8); ptr += 8;
                    values.push_back(Value::Double(v));
                    break;
                }
                case LogicalTypeId::BOOLEAN: {
                    if (ptr + 1 > end) {
                        values.push_back(Value::null());
                        ptr = end;
                        continue;
                    }
                    uint8_t v = 0;
                    std::memcpy(&v, ptr, 1); ptr += 1;
                    values.push_back(Value::boolean(v != 0));
                    break;
                }
                case LogicalTypeId::VARCHAR: {
                    // Check bounds for length field
                    if (ptr + 4 > end) {
                        values.push_back(Value::null());
                        ptr = end;
                        continue;
                    }
                    uint32_t len = 0;
                    std::memcpy(&len, ptr, 4); ptr += 4;
                    // Validate length doesn't exceed remaining buffer
                    if (len > static_cast<size_t>(end - ptr)) {
                        values.push_back(Value::null());
                        ptr = end;
                        continue;
                    }
                    std::string s(ptr, len); ptr += len;
                    values.push_back(Value::varchar(std::move(s)));
                    break;
                }
                default:
                    values.push_back(Value::null());
                    break;
            }
        }

        return Row(std::move(values));
    }
};

}  // namespace seeddb

#endif  // SEEDDB_STORAGE_ROW_SERIALIZER_H
