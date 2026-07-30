#include <gtest/gtest.h>

#include "queue/queue_manager.h"

using namespace remo::queue;

TEST(QueueManagerTest, OrdersByPriority) {
    QueueManager queue;
    queue.enqueue(1, "low.bin", 0);
    queue.enqueue(2, "high.bin", 10);

    const auto items = queue.getQueue();
    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].id, 2);
    EXPECT_EQ(items[1].id, 1);
}

TEST(QueueManagerTest, FreezeExcludesFromRunnableList) {
    QueueManager queue;
    queue.enqueue(1, "file.bin", 0);
    queue.freeze(1);

    EXPECT_TRUE(queue.getActiveDownloads().empty());
}
