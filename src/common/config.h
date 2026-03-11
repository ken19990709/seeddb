#ifndef SEEDDB_COMMON_CONFIG_H
#define SEEDDB_COMMON_CONFIG_H

#include <string>
#include <unordered_map>
#include <mutex>

namespace seeddb {

/// Configuration manager - loads and stores configuration values
class Config {
public:
    Config();
    ~Config() = default;

    /// Load configuration from file
    /// Returns true on success, false on failure
    bool load(const std::string& path);

    /// Get a string value, returning default if not found
    std::string get(const std::string& key, const std::string& default_value) const;

    /// Get an integer value, returning default if not found or invalid
    int get_int(const std::string& key, int default_value) const;

    /// Get a boolean value, returning default if not found
    bool get_bool(const std::string& key, bool default_value) const;

    /// Set a configuration value
    void set(const std::string& key, const std::string& value);

    // =========================================================================
    // Common configuration accessors
    // =========================================================================

    int port() const { return get_int("port", 5432); }
    int max_connections() const { return get_int("max_connections", 100); }
    std::string log_level() const { return get("log_level", "INFO"); }
    std::string data_directory() const { return get("data_directory", "./data"); }
    std::string listen_address() const { return get("listen_address", "0.0.0.0"); }
    int buffer_pool_size() const { return get_int("buffer_pool_size", 1024); }
    int page_size() const { return get_int("page_size", 8192); }

private:
    void set_defaults();

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> values_;
};

} // namespace seeddb

#endif // SEEDDB_COMMON_CONFIG_H
