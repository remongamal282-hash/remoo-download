#ifndef REMO_COMMON_LOGGING_H
#define REMO_COMMON_LOGGING_H

#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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

    static std::shared_ptr<spdlog::logger> get();

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace common
} // namespace remo

#define REMO_LOG_DEBUG(msg) remo::common::Logger::debug(msg)
#define REMO_LOG_INFO(msg) remo::common::Logger::info(msg)
#define REMO_LOG_WARNING(msg) remo::common::Logger::warning(msg)
#define REMO_LOG_ERROR(msg) remo::common::Logger::error(msg)
#define REMO_LOG_CRITICAL(msg) remo::common::Logger::critical(msg)

#endif // REMO_COMMON_LOGGING_H