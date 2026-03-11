#include "common/config.h"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace seeddb {

Config::Config() {
    set_defaults();
}

void Config::set_defaults() {
    values_["port"] = "5432";
    values_["max_connections"] = "100";
    values_["log_level"] = "INFO";
    values_["data_directory"] = "./data";
    values_["listen_address"] = "0.0.0.0";
    values_["buffer_pool_size"] = "1024";
    values_["page_size"] = "8192";
}

bool Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::string line;
    while (std::getline(file, line)) {
        // Remove comments (anything after #)
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        line = trim(line);

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;  // Skip malformed lines
        }

        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        if (!key.empty()) {
            values_[key] = value;
        }
    }

    return true;
}

std::string Config::get(const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it != values_.end()) {
        return it->second;
    }
    return default_value;
}

int Config::get_int(const std::string& key, int default_value) const {
    std::string value = get(key, "");
    if (value.empty()) {
        return default_value;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

bool Config::get_bool(const std::string& key, bool default_value) const {
    std::string value = get(key, "");
    if (value.empty()) {
        return default_value;
    }

    // Convert to lowercase for comparison
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return default_value;
}

void Config::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    values_[key] = value;
}

std::string Config::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace seeddb
