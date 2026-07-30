#ifndef REMO_DOWNLOAD_SCHEDULER_SCHEDULE_ENGINE_H
#define REMO_DOWNLOAD_SCHEDULER_SCHEDULE_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace scheduler {

struct Schedule {
    int64_t id = 0;
    std::string name;
    std::string scheduleType;
    std::string startTime;
    std::string endTime;
    std::string daysOfWeek;
    int dayOfMonth = 0;
    bool isActive = true;
    std::string quietHoursStart;
    std::string quietHoursEnd;
};

struct ScheduledDownload {
    int64_t scheduleId = 0;
    int64_t downloadId = 0;
};

class ScheduleEngine {
public:
    ScheduleEngine();
    ~ScheduleEngine();

    int64_t addSchedule(const Schedule& schedule);
    bool removeSchedule(int64_t id);
    bool updateSchedule(int64_t id, const Schedule& schedule);
    std::vector<Schedule> getActiveSchedules() const;
    bool isWithinQuietHours() const;
    bool shouldRunNow(const Schedule& schedule) const;
    void checkAndTrigger();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace scheduler
} // namespace remo

#endif // REMO_DOWNLOAD_SCHEDULER_SCHEDULE_ENGINE_H