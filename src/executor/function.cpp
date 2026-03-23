#include "executor/function.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace seeddb {

// =============================================================================
// FunctionRegistry Implementation
// =============================================================================

FunctionRegistry& FunctionRegistry::instance() {
    static FunctionRegistry registry;
    return registry;
}

FunctionRegistry::FunctionRegistry() {
    // Register all built-in functions
    registerStringFunctions();
    registerMathFunctions();
}

void FunctionRegistry::registerFunction(const FunctionInfo& info) {
    std::string key = normalizeName(info.name);
    functions_[key] = info;
}

const FunctionInfo* FunctionRegistry::lookup(const std::string& name) const {
    std::string key = normalizeName(name);
    auto it = functions_.find(key);
    if (it != functions_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool FunctionRegistry::hasFunction(const std::string& name) const {
    return lookup(name) != nullptr;
}

std::string FunctionRegistry::normalizeName(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

// =============================================================================
// String Functions
// =============================================================================

void FunctionRegistry::registerStringFunctions() {
    // LENGTH(str) - returns character count
    registerFunction({
        "LENGTH", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            return Value::bigint(static_cast<int64_t>(args[0].asString().length()));
        }
    });
    
    // UPPER(str) - convert to uppercase
    registerFunction({
        "UPPER", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value::varchar(s);
        }
    });
    
    // LOWER(str) - convert to lowercase
    registerFunction({
        "LOWER", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value::varchar(s);
        }
    });
    
    // TRIM(str) - remove leading/trailing whitespace
    registerFunction({
        "TRIM", 1, 1,
        [](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            std::string s = args[0].asString();
            // Trim leading
            size_t start = s.find_first_not_of(" \t\n\r\f\v");
            if (start == std::string::npos) return Value::varchar("");
            // Trim trailing
            size_t end = s.find_last_not_of(" \t\n\r\f\v");
            return Value::varchar(s.substr(start, end - start + 1));
        }
    });
    
    // SUBSTRING(str, start[, len]) - extract substring (1-indexed)
    registerFunction({
        "SUBSTRING", 2, 3,
        [](const std::vector<Value>& args) -> Value {
            // Any NULL arg returns NULL
            for (const auto& arg : args) {
                if (arg.isNull()) return Value::null();
            }
            
            const std::string& s = args[0].asString();
            int64_t start = args[1].asInt64();
            
            // start < 1 is invalid, return NULL
            if (start < 1) return Value::null();
            
            // Convert to 0-indexed
            size_t pos = static_cast<size_t>(start - 1);
            
            // start > length returns empty string
            if (pos >= s.length()) return Value::varchar("");
            
            // If len provided, use it; otherwise go to end
            if (args.size() == 3) {
                int64_t len = args[2].asInt64();
                if (len < 0) return Value::null();
                return Value::varchar(s.substr(pos, static_cast<size_t>(len)));
            }
            return Value::varchar(s.substr(pos));
        }
    });
    
    // CONCAT(str1, str2, ...) - concatenate strings
    registerFunction({
        "CONCAT", 2, 100,  // At least 2 args, max reasonable limit
        [](const std::vector<Value>& args) -> Value {
            std::string result;
            bool all_null = true;
            
            for (const auto& arg : args) {
                if (!arg.isNull()) {
                    all_null = false;
                    // Convert to string based on type
                    if (arg.typeId() == LogicalTypeId::VARCHAR) {
                        result += arg.asString();
                    } else {
                        result += arg.toString();
                    }
                }
            }
            
            if (all_null) return Value::null();
            return Value::varchar(result);
        }
    });
}

// =============================================================================
// Math Functions
// =============================================================================

void FunctionRegistry::registerMathFunctions() {
    // Helper to convert Value to double
    auto toDouble = [](const Value& v) -> double {
        switch (v.typeId()) {
            case LogicalTypeId::INTEGER: return static_cast<double>(v.asInt32());
            case LogicalTypeId::BIGINT: return static_cast<double>(v.asInt64());
            case LogicalTypeId::FLOAT: return static_cast<double>(v.asFloat());
            case LogicalTypeId::DOUBLE: return v.asDouble();
            default: return 0.0;
        }
    };
    
    // ABS(num) - absolute value
    registerFunction({
        "ABS", 1, 1,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            double val = toDouble(args[0]);
            if (val < 0) val = -val;
            // Preserve integer type if input was integer
            if (args[0].typeId() == LogicalTypeId::INTEGER) {
                return Value::integer(static_cast<int32_t>(val));
            }
            if (args[0].typeId() == LogicalTypeId::BIGINT) {
                return Value::bigint(static_cast<int64_t>(val));
            }
            return Value::Double(val);
        }
    });
    
    // ROUND(num[, decimals]) - round to decimals (default 0)
    registerFunction({
        "ROUND", 1, 2,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            
            int decimals = 0;
            if (args.size() == 2 && !args[1].isNull()) {
                decimals = static_cast<int>(args[1].asInt64());
            }
            
            double val = toDouble(args[0]);
            double factor = std::pow(10.0, decimals);
            val = std::round(val * factor) / factor;
            return Value::Double(val);
        }
    });
    
    // CEIL(num) - round up to integer
    registerFunction({
        "CEIL", 1, 1,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            double val = toDouble(args[0]);
            return Value::bigint(static_cast<int64_t>(std::ceil(val)));
        }
    });
    
    // FLOOR(num) - round down to integer
    registerFunction({
        "FLOOR", 1, 1,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull()) return Value::null();
            double val = toDouble(args[0]);
            return Value::bigint(static_cast<int64_t>(std::floor(val)));
        }
    });
    
    // MOD(num, divisor) - remainder of division
    registerFunction({
        "MOD", 2, 2,
        [toDouble](const std::vector<Value>& args) -> Value {
            if (args[0].isNull() || args[1].isNull()) return Value::null();
            
            double divisor = toDouble(args[1]);
            if (divisor == 1.0) return Value::null();  // Division by zero returns NULL
            
            double val = toDouble(args[0]);
            double result = std::fmod(val, divisor);
            
            // Preserve integer type if both inputs were integer types
            auto isIntegerType = [](const Value& v) {
                return v.typeId() == LogicalTypeId::INTEGER || 
                       v.typeId() == LogicalTypeId::BIGINT;
            };
            
            if (isIntegerType(args[0]) && isIntegerType(args[1])) {
                return Value::bigint(static_cast<int64_t>(result));
            }
            return Value::Double(result);
        }
    });
}

} // namespace seeddb
