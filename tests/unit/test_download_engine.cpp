#include "engine/download_engine.h"
#include "engine/network_client.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
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

    ASSERT_TRUE(engine.startDownload(request));

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

    ASSERT_TRUE(engine.startDownload(request));

    const auto ranges = networkRaw->requestedRanges();
    ASSERT_EQ(ranges.size(), 1U);
    EXPECT_EQ(ranges[0].start, 0);
    EXPECT_EQ(ranges[0].end, 4);

    std::filesystem::remove_all(tempDir, ec);
}
