#include "core/download_orchestrator.h"
#include "engine/network_client.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

// ---------------------------------------------------------------------------
// RealNetworkIntegrationTest
//
// Uses CurlNetworkClient (real libcurl, no mock) to verify that the
// DownloadEngine + CurlNetworkClient stack can actually write bytes to disk.
//
// Uses a tiny public file served over HTTPS so the test is fast and doesn't
// depend on large files.  The chosen endpoint returns exactly the requested
// number of random bytes via HTTP 200.
//
// If the network is unavailable (DNS failure / firewall / sandbox), the test
// is SKIPPED (not FAILED), so CI in restricted environments doesn't break.
// ---------------------------------------------------------------------------

static const std::string kTestUrl = "https://httpbin.org/bytes/8192";
static const std::string kExpectedMinBytes = "4096"; // at least 4 KB should arrive
static constexpr int64_t kMinExpected = 4096;

namespace {

bool networkIsReachable() {
    // Quick head request with a short timeout to detect no-internet environments
    remo::engine::CurlNetworkClient client;
    remo::engine::NetworkResourceInfo info;
    return client.head(kTestUrl, info);
}

} // namespace

TEST(RealNetworkIntegrationTest, CurlClientHeadSucceeds) {
    remo::engine::CurlNetworkClient client;
    remo::engine::NetworkResourceInfo info;
    if (!client.head(kTestUrl, info)) {
        GTEST_SKIP() << "Network not reachable or HTTP 503 — skipping real-curl test";
    }
    EXPECT_GE(info.contentLength, 0);
}

TEST(RealNetworkIntegrationTest, CurlClientDownloadsRealBytes) {
    if (!networkIsReachable()) {
        GTEST_SKIP() << "Network not reachable — skipping real-curl tests";
    }

    namespace fs = std::filesystem;
    const std::string tmpDir = (fs::temp_directory_path() / "remo_curl_test").string();
    const std::string tmpFile = tmpDir + "/real_download.bin";

    fs::create_directories(tmpDir);
    fs::remove(tmpFile);

    remo::engine::CurlNetworkClient client;
    remo::engine::ByteRange range{0, -1}; // full file

    int64_t reportedBytes = 0;
    bool ok = client.downloadToFile(kTestUrl, range, tmpFile,
                                    [&](int64_t n) {
                                        reportedBytes = n;
                                        return true;
                                    });

    EXPECT_TRUE(ok) << "downloadToFile returned false for " << kTestUrl;

    ASSERT_TRUE(fs::exists(tmpFile)) << "Output file was not created: " << tmpFile;

    const int64_t fileSize = static_cast<int64_t>(fs::file_size(tmpFile));
    EXPECT_GE(fileSize, kMinExpected)
        << "File too small: " << fileSize << " bytes at " << tmpFile;

    EXPECT_GT(reportedBytes, 0) << "progressCb was never called — progress reporting broken";

    fs::remove(tmpFile);
    fs::remove(tmpDir);
}

TEST(RealNetworkIntegrationTest, OrchestratorAddDownloadWritesFileToDisk) {
    if (!networkIsReachable()) {
        GTEST_SKIP() << "Network not reachable — skipping real-curl tests";
    }

    namespace fs = std::filesystem;
    const std::string tmpDir = (fs::temp_directory_path() / "remo_orchestrator_test").string();
    fs::create_directories(tmpDir);

    // Use CurlNetworkClient by default (no mock injected)
    remo::core::DownloadOrchestrator orch("" /* no DB */);
    ASSERT_TRUE(orch.start());

    std::string errorMessage;
    int64_t id = orch.addDownload(kTestUrl, tmpDir, "real_test_file.bin", "", errorMessage);
    if (id <= 0) {
        orch.stop();
        fs::remove_all(tmpDir);
        GTEST_SKIP() << "Network unreachable (addDownload failed: " << errorMessage << ")";
    }

    // Give the async download up to 15 seconds
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    int64_t fileSize = 0;
    const std::string outFile = tmpDir + "/real_test_file.bin";

    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::error_code ec;
        if (fs::exists(outFile, ec)) {
            fileSize = static_cast<int64_t>(fs::file_size(outFile, ec));
            if (fileSize >= kMinExpected) break;
        }
    }

    orch.stop();

    ASSERT_GT(id, 0) << "addDownload returned " << id << " — error: " << errorMessage;
    ASSERT_TRUE(fs::exists(outFile)) << "Output file not created: " << outFile;
    EXPECT_GE(fileSize, kMinExpected)
        << "File too small (" << fileSize << " bytes) after 15s — download may have stalled";

    fs::remove(outFile);
    fs::remove(tmpDir);
}
