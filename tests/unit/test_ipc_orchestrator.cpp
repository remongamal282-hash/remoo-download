#include "core/download_orchestrator.h"
#include "engine/network_client.h"
#include "ipc/named_pipe_ipc.h"
#include "storage/storage_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<char> bytesFromString(const std::string& value) {
    return std::vector<char>(value.begin(), value.end());
}

} // namespace

TEST(IpcOrchestratorTest, AddAndPauseDownloadViaIpc) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_ipc_test_1";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    const std::filesystem::path dbPath = tempDir / "ipc_test1.db";
    const std::string pipeName = "remo_test_ipc_1";

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 1000;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString(std::string(1000, 'X')));
    network->setChunkDelay(std::chrono::milliseconds(50));

    remo::core::DownloadOrchestrator orchestrator(dbPath.string(), std::move(network));
    ASSERT_TRUE(orchestrator.start());

    remo::ipc::NamedPipeIpcServer server;
    server.setMessageHandler([&orchestrator](const std::string& req) {
        return orchestrator.processRequest(req);
    });
    ASSERT_TRUE(server.start(pipeName));

    remo::ipc::NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));

    std::string addReq = "{\"command\":\"addDownload\",\"url\":\"https://example.test/file.zip\",\"savePath\":\"" +
                         tempDir.string() + "\",\"filename\":\"file.zip\"}";
    std::string addResp = client.sendRequest(addReq);
    EXPECT_NE(addResp.find("\"success\":true"), std::string::npos);
    EXPECT_NE(addResp.find("\"downloadId\":1"), std::string::npos);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    std::string pauseReq = "{\"command\":\"pauseDownload\",\"downloadId\":1}";
    std::string pauseResp = client.sendRequest(pauseReq);
    EXPECT_NE(pauseResp.find("\"success\":true"), std::string::npos);

    std::string statusReq = "{\"command\":\"getStatus\",\"downloadId\":1}";
    std::string statusResp = client.sendRequest(statusReq);
    EXPECT_NE(statusResp.find("\"status\":\"paused\""), std::string::npos);

    client.disconnect();
    server.stop();
    orchestrator.stop();
    std::filesystem::remove_all(tempDir, ec);
}

TEST(IpcOrchestratorTest, ResumeAndCancelDownloadViaIpc) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_ipc_test_2";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    const std::filesystem::path dbPath = tempDir / "ipc_test2.db";
    const std::string pipeName = "remo_test_ipc_2";

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 50000;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString(std::string(50000, 'Y')));
    network->setChunkDelay(std::chrono::milliseconds(300));

    remo::core::DownloadOrchestrator orchestrator(dbPath.string(), std::move(network));
    ASSERT_TRUE(orchestrator.start());

    remo::ipc::NamedPipeIpcServer server;
    server.setMessageHandler([&orchestrator](const std::string& req) {
        return orchestrator.processRequest(req);
    });
    ASSERT_TRUE(server.start(pipeName));

    remo::ipc::NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));

    std::string addReq = "{\"command\":\"addDownload\",\"url\":\"https://example.test/test2.bin\",\"savePath\":\"" +
                         tempDir.string() + "\",\"filename\":\"test2.bin\"}";
    client.sendRequest(addReq);

    std::string pauseReq = "{\"command\":\"pauseDownload\",\"downloadId\":1}";
    client.sendRequest(pauseReq);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resumeReq = "{\"command\":\"resumeDownload\",\"downloadId\":1}";
    std::string resumeResp = client.sendRequest(resumeReq);
    EXPECT_NE(resumeResp.find("\"success\":true"), std::string::npos);

    std::string cancelReq = "{\"command\":\"cancelDownload\",\"downloadId\":1}";
    std::string cancelResp = client.sendRequest(cancelReq);
    EXPECT_NE(cancelResp.find("\"success\":true"), std::string::npos);

    std::string statusReq = "{\"command\":\"getStatus\",\"downloadId\":1}";
    std::string statusResp = client.sendRequest(statusReq);
    EXPECT_NE(statusResp.find("\"status\":\"cancelled\""), std::string::npos);

    client.disconnect();
    server.stop();
    orchestrator.stop();
    std::filesystem::remove_all(tempDir, ec);
}

TEST(IpcOrchestratorTest, StartupRecoveryViaOrchestrator) {
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "remo_ipc_test_3";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    const std::filesystem::path dbPath = tempDir / "ipc_recovery.db";
    const std::string pipeName = "remo_test_ipc_3";

    {
        remo::storage::StorageManager storage(dbPath.string());
        ASSERT_TRUE(storage.open());

        remo::storage::DownloadRecord dlRecord;
        dlRecord.url = "https://example.test/recover_ipc.bin";
        dlRecord.filename = "recover_ipc.bin";
        dlRecord.savePath = tempDir.string();
        dlRecord.status = "downloading";
        dlRecord.totalSizeBytes = 50;
        int64_t id = storage.saveDownload(dlRecord);

        remo::storage::SegmentRecord seg;
        seg.downloadId = id;
        seg.segmentIndex = 0;
        seg.rangeStart = 0;
        seg.rangeEnd = 24;
        seg.downloadedBytes = 25;
        seg.status = "active";
        ASSERT_TRUE(storage.saveSegment(seg));

        std::ofstream out(tempDir / "recover_ipc.bin.part0", std::ios::binary);
        out.write("1234567890123456789012345", 25);
        out.close();

        storage.close();
    }

    auto network = std::make_unique<remo::engine::MockNetworkClient>();
    remo::engine::NetworkResourceInfo info;
    info.contentLength = 50;
    info.supportsRanges = true;
    network->setResourceInfo(info);
    network->setPayload(bytesFromString("1234567890123456789012345ABCDEFGHIJKLMNOPQRSTUVWXY"));

    remo::core::DownloadOrchestrator orchestrator(dbPath.string(), std::move(network));
    ASSERT_TRUE(orchestrator.start()); // triggers recoverUnfinishedDownloads()

    remo::ipc::NamedPipeIpcServer server;
    server.setMessageHandler([&orchestrator](const std::string& req) {
        return orchestrator.processRequest(req);
    });
    ASSERT_TRUE(server.start(pipeName));

    remo::ipc::NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));

    std::string statusReq = "{\"command\":\"getStatus\",\"downloadId\":1}";
    std::string statusResp = client.sendRequest(statusReq);

    EXPECT_NE(statusResp.find("\"status\":\"queued\""), std::string::npos);
    EXPECT_NE(statusResp.find("\"downloadedBytes\":25"), std::string::npos);

    client.disconnect();
    server.stop();
    orchestrator.stop();
    std::filesystem::remove_all(tempDir, ec);
}
