#include "common/logging.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace remo {
namespace common {
namespace {

int levelWeight(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return 0;
        case LogLevel::Info: return 1;
        case LogLevel::Warning: return 2;
        case LogLevel::Error: return 3;
        case LogLevel::Critical: return 4;
    }
    return 1;
}

const char* levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
    }
    return "INFO";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

} // namespace

std::unique_ptr<std::ofstream> Logger::s_fileStream = nullptr;
std::ostream* Logger::s_stream = &std::clog;
std::mutex Logger::s_mutex;
LogLevel Logger::s_level = LogLevel::Info;
std::string Logger::s_appName = "RemooDownload";

void Logger::init(const std::string& appName, const std::string& logDir) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_appName = appName.empty() ? "RemooDownload" : appName;
    s_stream = &std::clog;
    s_fileStream.reset();

    if (!logDir.empty()) {
        const std::string logPath = logDir + "/" + s_appName + ".log";
        auto file = std::make_unique<std::ofstream>(logPath, std::ios::app);
        if (file->good()) {
            s_stream = file.get();
            s_fileStream = std::move(file);
        }
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_fileStream.reset();
    s_stream = &std::clog;
}

void Logger::setLevel(LogLevel level) {
    s_level = level;
}

LogLevel Logger::level() {
    return s_level;
}

void Logger::debug(const std::string& msg) {
    Logger::write(LogLevel::Debug, msg);
}

void Logger::info(const std::string& msg) {
    Logger::write(LogLevel::Info, msg);
}

void Logger::warning(const std::string& msg) {
    Logger::write(LogLevel::Warning, msg);
}

void Logger::error(const std::string& msg) {
    Logger::write(LogLevel::Error, msg);
}

void Logger::critical(const std::string& msg) {
    Logger::write(LogLevel::Critical, msg);
}

std::ostream* Logger::stream() {
    return s_stream;
}

void Logger::write(LogLevel level, const std::string& msg) {
    if (levelWeight(level) < levelWeight(s_level)) {
        return;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_stream) {
        return;
    }
    (*s_stream) << timestamp() << " [" << levelName(level) << "] "
                << s_appName << ": " << msg << '\n';
    s_stream->flush();
}

} // namespace common
} // namespace remo
