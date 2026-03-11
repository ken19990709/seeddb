#ifndef SEEDDB_COMMON_LOGGER_H
#define SEEDDB_COMMON_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace seeddb {

/// Log severity levels
enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
    FATAL = 4,
};

/// Thread-safe logger with file output
class Logger {
public:
    /// Get singleton instance
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    /// Set minimum log level (thread-safe, lock-free)
    void set_level(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
    }

    /// Get current log level (thread-safe, lock-free)
    LogLevel level() const {
        return level_.load(std::memory_order_relaxed);
    }

    /// Open log file for writing
    bool open_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
        file_.open(path, std::ios::app);
        return file_.is_open();
    }

    /// Close log file
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
    }

    /// Flush buffered output
    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
    }

    /// Log a message at the given level
    void log(LogLevel level, const std::string& message) {
        // Fast path: atomic check without lock (optimization for filtered messages)
        if (level < level_.load(std::memory_order_relaxed)) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        std::string timestamp = get_timestamp();
        std::string level_str = level_to_string(level);

        // Format: [TIMESTAMP] [LEVEL] message
        std::string line = "[" + timestamp + "] [" + level_str + "] " + message + "\n";

        // Write to file if open
        if (file_.is_open()) {
            file_ << line;
        }

        // Also write to stderr for ERROR and FATAL
        if (level >= LogLevel::ERROR) {
            std::cerr << line;
        }
    }

    /// Convenience methods
    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message)  { log(LogLevel::INFO, message); }
    void warn(const std::string& message)  { log(LogLevel::WARN, message); }
    void error(const std::string& message) { log(LogLevel::ERROR, message); }
    void fatal(const std::string& message) { log(LogLevel::FATAL, message); }

private:
    Logger() : level_(LogLevel::INFO), file_() {}
    ~Logger() { close(); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf;
        localtime_r(&time, &tm_buf);

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);

        std::ostringstream oss;
        oss << buffer << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    const char* level_to_string(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }

    std::atomic<LogLevel> level_;
    std::ofstream file_;
    mutable std::mutex mutex_;
};

} // namespace seeddb

// Convenience macros
#define SEEDDB_DEBUG(msg) seeddb::Logger::instance().debug(msg)
#define SEEDDB_INFO(msg)  seeddb::Logger::instance().info(msg)
#define SEEDDB_WARN(msg)  seeddb::Logger::instance().warn(msg)
#define SEEDDB_ERROR(msg) seeddb::Logger::instance().error(msg)
#define SEEDDB_FATAL(msg) seeddb::Logger::instance().fatal(msg)

#endif // SEEDDB_COMMON_LOGGER_H
