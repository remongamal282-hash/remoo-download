#include "common/logging.h"

namespace remo {
namespace common {

std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;

void Logger::init(const std::string& appName, const std::string& logDir) {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

        if (!logDir.empty()) {
            std::string logPath = logDir + "/" + appName + ".log";
            sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true));
        }

        auto combinedSink = std::make_shared<spdlog::sinks::combined_sink>(sinks);
        s_logger = std::make_shared<spdlog::logger>(appName, combinedSink);
        s_logger->set_level(spdlog::level::debug);
        s_logger->flush_on(spdlog::level::err);

        spdlog::register_logger(s_logger);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger init failed: " << ex.what() << std::endl;
    }
}

void Logger::shutdown() {
    s_logger.reset();
    spdlog::drop_all();
}

void Logger::setLevel(LogLevel level) {
    if (!s_logger) return;
    switch (level) {
        case LogLevel::Debug: s_logger->set_level(spdlog::level::debug); break;
        case LogLevel::Info: s_logger->set_level(spdlog::level::info); break;
        case LogLevel::Warning: s_logger->set_level(spdlog::level::warn); break;
        case LogLevel::Error: s_logger->set_level(spdlog::level::err); break;
        case LogLevel::Critical: s_logger->set_level(spdlog::level::critical); break;
    }
}

LogLevel Logger::level() {
    if (!s_logger) return LogLevel::Info;
    auto lvl = s_logger->level();
    if (lvl == spdlog::level::debug) return LogLevel::Debug;
    if (lvl == spdlog::level::info) return LogLevel::Info;
    if (lvl == spdlog::level::warn) return LogLevel::Warning;
    if (lvl == spdlog::level::err) return LogLevel::Error;
    return LogLevel::Critical;
}

void Logger::debug(const std::string& msg) {
    if (s_logger) s_logger->debug(msg);
}

void Logger::info(const std::string& msg) {
    if (s_logger) s_logger->info(msg);
}

void Logger::warning(const std::string& msg) {
    if (s_logger) s_logger->warn(msg);
}

void Logger::error(const std::string& msg) {
    if (s_logger) s_logger->error(msg);
}

void Logger::critical(const std::string& msg) {
    if (s_logger) s_logger->critical(msg);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return s_logger;
}

} // namespace common
} // namespace remo