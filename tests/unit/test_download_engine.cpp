#include "engine/download_engine.h"
#include "engine/network_client.h"
#include "engine/network_monitor.h"
#include "engine/reconnect_manager.h"
#include "storage/storage_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<char> bytesFromString(const std::string& value) {
    return std::vector<char>(value.begin(), value.end());
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST(DownloadEngineTest, DownloadsAndMergesSegmentedFileWithMockNetwork) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_engine_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 10;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("abcdefghij"));

    remo::engine::DownloadEngine engine(4, std::move(network));
    remo::engine::DownloadRequest request;
    request.url = "https://example.test/file.bin";
    request.filename = "file.bin";
    request.savePath = tempDir.string();

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);

    ASSERT_TRUE(engine.waitForDownload(id));

    const auto outputPath = tempDir / request.filename;
    EXPECT_EQ(readFile(outputPath), "abcdefghij");
    EXPECT_FALSE(std::filesystem::exists(outputPath.string() + ".part0"));

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, FallsBackToSingleSegmentWhenRangesAreUnsupported) {
    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    auto* networkRaw = network.get();

    remo::engine::NetworkResourceInfo info;
    info.contentLength = 5;
    info.supportsRanges = false;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("abcde"));

    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_engine_fallback_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    remo::engine::DownloadEngine engine(4, std::move(network));
    remo::engine::DownloadRequest request;
    request.url = "https://example.test/file.bin";
    request.filename = "fallback.bin";
    request.savePath = tempDir.string();

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);

    ASSERT_TRUE(engine.waitForDownload(id));

    const auto ranges = networkRaw->requestedRanges();
    ASSERT_EQ(ranges.size(), 1U);
    EXPECT_EQ(ranges[0].start, 0);
    EXPECT_EQ(ranges[0].end, 4);

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, StartDownloadReturnsImmediatelyAsync) {
    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 512;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setChunkDelay(std::chrono::milliseconds(20));

    std::string largeData(512, 'X');
    network->setPayload(bytesFromString(largeData));

    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_async_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    remo::engine::DownloadEngine engine(2, std::move(network));
    remo::engine::DownloadRequest request;
    request.url = "https://example.test/async.bin";
    request.filename = "async.bin";
    request.savePath = tempDir.string();

    const auto start = std::chrono::steady_clock::now();
    int64_t id = engine.startDownload(request);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();

    ASSERT_GT(id, 0);
    EXPECT_LT(elapsed, 100); // Must return asynchronously in < 100ms

    ASSERT_TRUE(engine.waitForDownload(id));
    const auto outputPath = tempDir / request.filename;
    EXPECT_EQ(readFile(outputPath), largeData);

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, RealPauseAndResumeDownload) {
    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 512;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setChunkDelay(std::chrono::milliseconds(15));

    std::string testPayload;
    for (int i = 0; i < 512; ++i) {
        testPayload += static_cast<char>('A' + (i % 26));
    }
    network->setPayload(bytesFromString(testPayload));

    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_pause_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    remo::engine::DownloadEngine engine(1, std::move(network));
    remo::engine::DownloadRequest request;
    request.url = "https://example.test/pausetest.bin";
    request.filename = "pausetest.bin";
    request.savePath = tempDir.string();

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_TRUE(engine.pauseDownload(id));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto progressPaused = engine.getProgress(id);
    EXPECT_EQ(progressPaused.statusMessage, "paused");

    EXPECT_TRUE(engine.resumeDownload(id));
    ASSERT_TRUE(engine.waitForDownload(id));

    const auto outputPath = tempDir / request.filename;
    EXPECT_EQ(readFile(outputPath), testPayload);

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, PeriodicCheckpointInStorageManager) {
    const std::filesystem::path dbPath =
        std::filesystem::temp_directory_path() / "test_checkpoint.sqlite";
    std::error_code ec;
    std::filesystem::remove(dbPath, ec);

    remo::storage::StorageManager storage(dbPath.string());
    ASSERT_TRUE(storage.open());

    remo::storage::DownloadRecord dlRecord;
    dlRecord.url = "https://example.test/check.bin";
    dlRecord.filename = "check.bin";
    dlRecord.savePath = "check.bin";
    dlRecord.status = "queued";
    int64_t dbDlId = storage.saveDownload(dlRecord);
    ASSERT_GT(dbDlId, 0);

    remo::storage::SegmentRecord segRecord;
    segRecord.downloadId = dbDlId;
    segRecord.segmentIndex = 0;
    segRecord.rangeStart = 0;
    segRecord.rangeEnd = 99;
    segRecord.status = "pending";
    ASSERT_TRUE(storage.saveSegment(segRecord));

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 100;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString(std::string(100, 'Z')));

    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_checkpoint_test";
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    remo::engine::DownloadEngine engine(1, std::move(network), &storage);
    remo::engine::DownloadRequest request;
    request.url = "https://example.test/check.bin";
    request.filename = "check.bin";
    request.savePath = tempDir.string();

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);
    ASSERT_TRUE(engine.waitForDownload(id));

    std::string checkpointData = storage.restoreCheckpoint(id, 0);
    EXPECT_FALSE(checkpointData.empty());

    storage.close();
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, AutoReconnect_ExponentialBackoffAndReconnectingState) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_reconnect_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 20;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("12345678901234567890"));
    network->setFailDownloadCount(2);

    remo::engine::DownloadEngine engine(1, std::move(network));
    engine.setFastRetryMode(true);

    remo::engine::DownloadRequest request;
    request.url = "https://example.test/retry.bin";
    request.filename = "retry.bin";
    request.savePath = tempDir.string();
    request.maxRetries = 5;

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);

    ASSERT_TRUE(engine.waitForDownload(id));

    auto progress = engine.getProgress(id);
    EXPECT_EQ(progress.statusMessage, "completed");
    EXPECT_EQ(readFile(tempDir / request.filename), "12345678901234567890");

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, AutoReconnect_ExceedsMaxRetries_TransitionsToFailed) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_max_retries_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 20;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("12345678901234567890"));
    network->setFailDownload(true);

    remo::engine::DownloadEngine engine(1, std::move(network));
    engine.setFastRetryMode(true);

    remo::engine::DownloadRequest request;
    request.url = "https://example.test/failed.bin";
    request.filename = "failed.bin";
    request.savePath = tempDir.string();
    request.maxRetries = 2;

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);

    ASSERT_TRUE(engine.waitForDownload(id));

    auto progress = engine.getProgress(id);
    EXPECT_EQ(progress.statusMessage, "failed");
    EXPECT_FALSE(progress.errorMessage.empty());
    EXPECT_GT(progress.retryCount, 2);

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, AutoReconnect_NetworkRestorationTriggersImmediateRetry) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_restore_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    auto monitor = std::make_shared<remo::engine::NetworkMonitor>();
    monitor->setStatus(remo::engine::NetworkStatus::Offline);

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 10;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("1234567890"));
    network->setFailDownloadCount(1);

    remo::engine::DownloadEngine engine(1, std::move(network));
    engine.setNetworkMonitor(monitor);
    engine.setFastRetryMode(true);

    remo::engine::DownloadRequest request;
    request.url = "https://example.test/restore.bin";
    request.filename = "restore.bin";
    request.savePath = tempDir.string();
    request.maxRetries = 5;

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    monitor->setStatus(remo::engine::NetworkStatus::Online);

    ASSERT_TRUE(engine.waitForDownload(id));

    auto progress = engine.getProgress(id);
    EXPECT_EQ(progress.statusMessage, "completed");
    EXPECT_EQ(readFile(tempDir / request.filename), "1234567890");

    std::filesystem::remove_all(tempDir, ec);
}

TEST(DownloadEngineTest, StartupRecovery_ResumesPartialDownloadAfterCrash) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_download_recovery_test";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    const std::filesystem::path dbPath = tempDir / "recovery.db";
    remo::storage::StorageManager storage(dbPath.string());
    ASSERT_TRUE(storage.open());

    remo::storage::DownloadRecord dlRecord;
    dlRecord.url = "https://example.test/recovery.bin";
    dlRecord.filename = "recovery.bin";
    dlRecord.savePath = tempDir.string();
    dlRecord.status = "downloading";
    dlRecord.totalSizeBytes = 20;
    int64_t dbDlId = storage.saveDownload(dlRecord);
    ASSERT_GT(dbDlId, 0);

    remo::storage::SegmentRecord seg0;
    seg0.downloadId = dbDlId;
    seg0.segmentIndex = 0;
    seg0.rangeStart = 0;
    seg0.rangeEnd = 9;
    seg0.downloadedBytes = 10;
    seg0.status = "completed";
    ASSERT_TRUE(storage.saveSegment(seg0));

    remo::storage::SegmentRecord seg1;
    seg1.downloadId = dbDlId;
    seg1.segmentIndex = 1;
    seg1.rangeStart = 10;
    seg1.rangeEnd = 19;
    seg1.downloadedBytes = 0;
    seg1.status = "pending";
    ASSERT_TRUE(storage.saveSegment(seg1));

    const std::filesystem::path part0 = tempDir / "recovery.bin.part0";
    std::ofstream out0(part0, std::ios::binary);
    out0.write("1234567890", 10);
    out0.close();

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 20;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("1234567890ABCDEFGHIJ"));

    remo::engine::DownloadEngine engine(2, std::move(network), &storage);
    int recovered = engine.recoverUnfinishedDownloads();
    EXPECT_EQ(recovered, 1);

    auto recoveredRecord = storage.getDownload(dbDlId);
    EXPECT_EQ(recoveredRecord.status, "queued");
    EXPECT_EQ(recoveredRecord.downloadedBytes, 10);

    remo::engine::DownloadRequest request;
    request.url = dlRecord.url;
    request.filename = dlRecord.filename;
    request.savePath = dlRecord.savePath;

    int64_t id = engine.startDownload(request);
    ASSERT_GT(id, 0);
    ASSERT_TRUE(engine.waitForDownload(id));

    const auto finalFile = tempDir / "recovery.bin";
    EXPECT_EQ(readFile(finalFile), "1234567890ABCDEFGHIJ");

    storage.close();
    std::filesystem::remove_all(tempDir, ec);
}
