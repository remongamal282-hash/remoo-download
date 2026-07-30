#include "scheduler/schedule_engine.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <mutex>

namespace remo {
namespace scheduler {

class ScheduleEngine::Impl {
public:
    std::mutex mutex;
    std::vector<Schedule> schedules;
    std::vector<ScheduledDownload> scheduledDownloads;
};

namespace {

int minutesFromMidnight(const std::string& value) {
    if (value.size() < 5) {
        return -1;
    }
    int hours = 0;
    int minutes = 0;
    char separator = '\0';
    std::istringstream stream(value);
    if (!(stream >> hours >> separator >> minutes) || separator != ':' ||
        hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
        return -1;
    }
    return hours * 60 + minutes;
}

int currentMinutesFromMidnight() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    return localTime.tm_hour * 60 + localTime.tm_min;
}

bool isWithinQuietHoursForSchedule(const Schedule& schedule, int now) {
    const int start = minutesFromMidnight(schedule.quietHoursStart);
    const int end = minutesFromMidnight(schedule.quietHoursEnd);
    if (start < 0 || end < 0 || start == end) {
        return false;
    }
    if (start < end) {
        return now >= start && now < end;
    }
    return now >= start || now < end;
}

bool shouldRunScheduleNow(const Schedule& schedule, int now) {
    if (!schedule.isActive || isWithinQuietHoursForSchedule(schedule, now)) {
        return false;
    }
    const int start = minutesFromMidnight(schedule.startTime);
    if (start < 0) {
        return true;
    }
    return now >= start;
}

} // namespace

ScheduleEngine::ScheduleEngine()
    : d(std::make_unique<Impl>())
{
}

ScheduleEngine::~ScheduleEngine() = default;

int64_t ScheduleEngine::addSchedule(const Schedule& schedule) {
    std::lock_guard<std::mutex> lock(d->mutex);
    Schedule s = schedule;
    s.id = d->schedules.empty() ? 1 : d->schedules.back().id + 1;
    d->schedules.push_back(s);
    return s.id;
}

bool ScheduleEngine::removeSchedule(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = std::remove_if(d->schedules.begin(), d->schedules.end(),
                             [id](const Schedule& s) { return s.id == id; });
    if (it != d->schedules.end()) {
        d->schedules.erase(it, d->schedules.end());
        return true;
    }
    return false;
}

bool ScheduleEngine::updateSchedule(int64_t id, const Schedule& schedule) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& s : d->schedules) {
        if (s.id == id) {
            s = schedule;
            s.id = id;
            return true;
        }
    }
    return false;
}

std::vector<Schedule> ScheduleEngine::getActiveSchedules() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    std::vector<Schedule> result;
    for (const auto& s : d->schedules) {
        if (s.isActive) {
            result.push_back(s);
        }
    }
    return result;
}

bool ScheduleEngine::isWithinQuietHours() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    const int now = currentMinutesFromMidnight();
    for (const auto& schedule : d->schedules) {
        if (!schedule.isActive) {
            continue;
        }
        if (isWithinQuietHoursForSchedule(schedule, now)) {
            return true;
        }
    }
    return false;
}

bool ScheduleEngine::shouldRunNow(const Schedule& schedule) const {
    return shouldRunScheduleNow(schedule, currentMinutesFromMidnight());
}

void ScheduleEngine::checkAndTrigger() {
    std::lock_guard<std::mutex> lock(d->mutex);
    const int now = currentMinutesFromMidnight();
    for (auto& schedule : d->schedules) {
        if (shouldRunScheduleNow(schedule, now)) {
            schedule.isActive = schedule.scheduleType == "recurring";
        }
    }
}

} // namespace scheduler
} // namespace remo
