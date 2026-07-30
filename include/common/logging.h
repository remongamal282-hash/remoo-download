#ifndef REMO_COMMON_LOGGING_H
#define REMO_COMMON_LOGGING_H

#include <memory>
#include <mutex>
#include <ostream>
#include <fstream>
#include <string>

namespace remo {
namespace common {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static void init(const std::string& appName, const std::string& logDir = "");
    static void shutdown();

    static void setLevel(LogLevel level);
    static LogLevel level();

    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warning(const std::string& msg);
    static void error(const std::string& msg);
    static void critical(const std::string& msg);

    static std::ostream* stream();

private:
    static void write(LogLevel level, const std::string& msg);

    static std::unique_ptr<std::ofstream> s_fileStream;
    static std::ostream* s_stream;
    static std::mutex s_mutex;
    static LogLevel s_level;
    static std::string s_appName;
};

} // namespace common
} // namespace remo

#define REMO_LOG_DEBUG(msg) remo::common::Logger::debug(msg)
#define REMO_LOG_INFO(msg) remo::common::Logger::info(msg)
#define REMO_LOG_WARNING(msg) remo::common::Logger::warning(msg)
#define REMO_LOG_ERROR(msg) remo::common::Logger::error(msg)
#define REMO_LOG_CRITICAL(msg) remo::common::Logger::critical(msg)

#endif // REMO_COMMON_LOGGING_H
