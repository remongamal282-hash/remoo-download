#include <cstdio>

#include <gtest/gtest.h>

#include "storage/storage_manager.h"

using namespace remo::storage;

TEST(StorageManagerTest, CreatesSchemaAndPersistsDownload) {
    const char* dbPath = "remo_test.sqlite";
    std::remove(dbPath);

    StorageManager storage(dbPath);
    ASSERT_TRUE(storage.open());
    EXPECT_EQ(storage.getSchemaVersion(), 1);

    DownloadRecord record;
    record.url = "https://example.com/file.zip";
    record.filename = "file.zip";
    record.savePath = "downloads/file.zip";
    record.status = "queued";

    const auto id = storage.saveDownload(record);
    ASSERT_GT(id, 0);

    const auto restored = storage.getDownload(id);
    EXPECT_EQ(restored.url, record.url);
    EXPECT_EQ(restored.filename, record.filename);

    storage.close();
    std::remove(dbPath);
    std::remove("remo_test.sqlite-wal");
    std::remove("remo_test.sqlite-shm");
}
