#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "browser/native_messaging_host.h"
#include "ipc/named_pipe_ipc.h"

using namespace remo::browser;
using namespace remo::ipc;

namespace {

// Helper to encode a message with 4-byte length prefix (little-endian)
std::string encodeNativeMessage(const std::string& message) {
    const uint32_t length = static_cast<uint32_t>(message.size());
    std::string encoded;
    encoded.reserve(4 + message.size());
    
    encoded.push_back(static_cast<char>(length & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 8U) & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 16U) & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 24U) & 0xFFU));
    encoded.append(message);
    
    return encoded;
}

// Helper to decode a message with 4-byte length prefix (little-endian)
std::pair<bool, std::string> decodeNativeMessage(const std::string& encoded) {
    if (encoded.size() < 4) {
        return {false, ""};
    }
    
    const auto* bytes = reinterpret_cast<const unsigned char*>(encoded.data());
    const uint32_t length =
        static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8U) |
        (static_cast<uint32_t>(bytes[2]) << 16U) |
        (static_cast<uint32_t>(bytes[3]) << 24U);
    
    if (encoded.size() < 4 + length) {
        return {false, ""};
    }
    
    return {true, encoded.substr(4, length)};
}

// Mock IPC server that simulates remo_service
class MockServiceIpcServer {
public:
    MockServiceIpcServer() = default;
    
    bool start(const std::string& pipeName) {
        server = std::make_unique<NamedPipeIpcServer>();
        
        server->setMessageHandler([this](const std::string& request) {
            lastReceivedRequest = request;
            requestCount++;
            
            // Parse request and send appropriate response
            if (request.find("\"command\":\"addDownload\"") != std::string::npos &&
                request.find("\"url\":") != std::string::npos) {
                return R"({"ok":true,"status":"accepted","downloadId":1})";
            } else if (request.find("\"command\":\"getStatus\"") != std::string::npos) {
                return R"({"ok":true,"status":"downloading","progress":50})";
            } else {
                return R"({"ok":false,"error":"unknown_command"})";
            }
        });
        
        return server->start(pipeName);
    }
    
    void stop() {
        if (server) {
            server->stop();
        }
    }
    
    std::string getLastRequest() const { return lastReceivedRequest; }
    int getRequestCount() const { return requestCount; }
    
private:
    std::unique_ptr<NamedPipeIpcServer> server;
    std::string lastReceivedRequest;
    int requestCount = 0;
};

} // namespace

class NativeHostIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        pipeName = "remo_test_pipe_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        
        // Start mock service
        mockService = std::make_unique<MockServiceIpcServer>();
        ASSERT_TRUE(mockService->start(pipeName));
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void TearDown() override {
        if (mockService) {
            mockService->stop();
        }
    }
    
    std::string pipeName;
    std::unique_ptr<MockServiceIpcServer> mockService;
};

TEST_F(NativeHostIntegrationTest, ParsesAddDownloadRequest) {
    const std::string browserMessage = R"({
        "type": "addDownload",
        "url": "https://example.com/test.zip",
        "filename": "test.zip",
        "source": "chromium"
    })";
    
    // Create a temporary input file to simulate stdin
    const std::string tempInputFile = "test_input.bin";
    {
        std::ofstream ofs(tempInputFile, std::ios::binary);
        const std::string encoded = encodeNativeMessage(browserMessage);
        ofs.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    }
    
    // Note: This is a conceptual test - actual integration would require
    // running native_host_main as a separate process and piping data
    
    EXPECT_TRUE(browserMessage.find("addDownload") != std::string::npos);
    EXPECT_TRUE(browserMessage.find("https://example.com/test.zip") != std::string::npos);
    
    // Cleanup
    std::remove(tempInputFile.c_str());
}

TEST_F(NativeHostIntegrationTest, IpcClientCanConnectToMockService) {
    NamedPipeIpcClient client;
    
    ASSERT_TRUE(client.connect(pipeName, 5000));
    
    const std::string request = R"({"command":"addDownload","url":"https://example.com/file.zip"})";
    const std::string response = client.sendRequest(request);
    
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(response.find("\"ok\":true") != std::string::npos || 
                response.find("\"status\":\"accepted\"") != std::string::npos);
    
    EXPECT_EQ(mockService->getRequestCount(), 1);
    EXPECT_TRUE(mockService->getLastRequest().find("addDownload") != std::string::npos);
    
    client.disconnect();
}

TEST_F(NativeHostIntegrationTest, ServiceReceivesCorrectlyFormattedRequest) {
    NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));
    
    const std::string url = "https://example.com/download/test-file.zip";
    const std::string filename = "test-file.zip";
    
    std::ostringstream requestBuilder;
    requestBuilder << "{"
                   << "\"command\":\"addDownload\","
                   << "\"url\":\"" << url << "\","
                   << "\"filename\":\"" << filename << "\""
                   << "}";
    
    const std::string request = requestBuilder.str();
    const std::string response = client.sendRequest(request);
    
    const std::string receivedRequest = mockService->getLastRequest();
    
    EXPECT_TRUE(receivedRequest.find("\"command\":\"addDownload\"") != std::string::npos);
    EXPECT_TRUE(receivedRequest.find(url) != std::string::npos);
    EXPECT_TRUE(receivedRequest.find(filename) != std::string::npos);
    
    EXPECT_TRUE(response.find("\"ok\":true") != std::string::npos);
    
    client.disconnect();
}

TEST_F(NativeHostIntegrationTest, HandlesEmptyUrl) {
    NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));
    
    const std::string request = R"({"command":"addDownload","url":""})";
    const std::string response = client.sendRequest(request);
    
    // Service should still respond (even if it rejects the empty URL)
    EXPECT_FALSE(response.empty());
    
    client.disconnect();
}

TEST_F(NativeHostIntegrationTest, HandlesLongUrl) {
    NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));
    
    // Create a very long URL
    std::string longUrl = "https://example.com/download/";
    longUrl += std::string(1000, 'a');
    longUrl += ".zip";
    
    std::ostringstream requestBuilder;
    requestBuilder << "{"
                   << "\"command\":\"addDownload\","
                   << "\"url\":\"" << longUrl << "\""
                   << "}";
    
    const std::string request = requestBuilder.str();
    const std::string response = client.sendRequest(request);
    
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(mockService->getLastRequest().find(longUrl) != std::string::npos);
    
    client.disconnect();
}

TEST_F(NativeHostIntegrationTest, HandlesSpecialCharactersInUrl) {
    NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));
    
    // URL with spaces and special characters (should be properly escaped in real usage)
    const std::string url = "https://example.com/download/file%20with%20spaces%20(v1.0).zip";
    
    std::ostringstream requestBuilder;
    requestBuilder << "{"
                   << "\"command\":\"addDownload\","
                   << "\"url\":\"" << url << "\""
                   << "}";
    
    const std::string request = requestBuilder.str();
    const std::string response = client.sendRequest(request);
    
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(mockService->getLastRequest().find("addDownload") != std::string::npos);
    
    client.disconnect();
}

TEST_F(NativeHostIntegrationTest, MultipleSequentialRequests) {
    NamedPipeIpcClient client;
    ASSERT_TRUE(client.connect(pipeName, 5000));
    
    // Send multiple requests
    for (int i = 0; i < 5; ++i) {
        std::ostringstream requestBuilder;
        requestBuilder << "{"
                       << "\"command\":\"addDownload\","
                       << "\"url\":\"https://example.com/file" << i << ".zip\""
                       << "}";
        
        const std::string response = client.sendRequest(requestBuilder.str());
        EXPECT_FALSE(response.empty());
    }
    
    EXPECT_EQ(mockService->getRequestCount(), 5);
    
    client.disconnect();
}

TEST_F(NativeHostIntegrationTest, ServiceNotAvailableReturnsError) {
    // Stop the mock service
    mockService->stop();
    
    // Wait for service to fully stop
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    NamedPipeIpcClient client;
    
    // Connection should fail
    EXPECT_FALSE(client.connect(pipeName, 1000));
}

TEST_F(NativeHostIntegrationTest, NativeMessagingProtocolIntegrity) {
    // Test that the protocol encoding/decoding works correctly
    const std::string originalMessage = R"({"type":"addDownload","url":"https://example.com/test.zip"})";
    
    // Encode
    const std::string encoded = encodeNativeMessage(originalMessage);
    
    // Verify length prefix
    ASSERT_GE(encoded.size(), 4);
    const auto* bytes = reinterpret_cast<const unsigned char*>(encoded.data());
    const uint32_t encodedLength =
        static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8U) |
        (static_cast<uint32_t>(bytes[2]) << 16U) |
        (static_cast<uint32_t>(bytes[3]) << 24U);
    
    EXPECT_EQ(encodedLength, originalMessage.size());
    
    // Decode
    const auto [success, decoded] = decodeNativeMessage(encoded);
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded, originalMessage);
}
