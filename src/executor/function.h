#ifndef SEEDDB_EXECUTOR_FUNCTION_H
#define SEEDDB_EXECUTOR_FUNCTION_h

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/value.h"

namespace seeddb {

/// Function signature - takes evaluated arguments, returns Value
using ScalarFunction = std::function<Value(const std::vector<Value>&)>;

/// Function metadata
struct FunctionInfo {
    std::string name;        // Function name (uppercase)
    size_t min_args;         // Minimum required arguments
    size_t max_args;         // Maximum allowed arguments
    ScalarFunction impl;     // Implementation
};

/// Function registry - static lookup table for scalar functions
class FunctionRegistry {
public:
    /// Get singleton instance
    static FunctionRegistry& instance();
    
    /// Register a function
    void registerFunction(const FunctionInfo& info);
    
    /// Lookup function by name (case-insensitive)
    /// Returns nullptr if function not found
    const FunctionInfo* lookup(const std::string& name) const;
    
    /// Check if function exists
    bool hasFunction(const std::string& name) const;
    
private:
    FunctionRegistry();
    
    /// Register all built-in string functions
    void registerStringFunctions();
    
    /// Register all built-in math functions
    void registerMathFunctions();
    
    /// Convert name to uppercase for case-insensitive lookup
    static std::string normalizeName(const std::string& name);
    
    std::unordered_map<std::string, FunctionInfo> functions_;
};

} // namespace seeddb

#endif // SEEDDB_EXECUTOR_FUNCTION_H
