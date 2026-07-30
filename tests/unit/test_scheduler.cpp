#include <gtest/gtest.h>

#include "scheduler/schedule_engine.h"

using namespace remo::scheduler;

TEST(ScheduleEngineTest, StoresActiveSchedules) {
    ScheduleEngine engine;
    Schedule schedule;
    schedule.name = "Night downloads";
    schedule.scheduleType = "one_time";
    schedule.isActive = true;

    const auto id = engine.addSchedule(schedule);
    ASSERT_GT(id, 0);

    const auto active = engine.getActiveSchedules();
    ASSERT_EQ(active.size(), 1U);
    EXPECT_EQ(active[0].name, "Night downloads");
}

TEST(ScheduleEngineTest, InactiveScheduleDoesNotRun) {
    ScheduleEngine engine;
    Schedule schedule;
    schedule.name = "Inactive";
    schedule.scheduleType = "one_time";
    schedule.isActive = false;

    EXPECT_FALSE(engine.shouldRunNow(schedule));
}
