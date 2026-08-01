#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <sstream>
#include <string>

#include "browser/native_messaging_host.h"

using namespace remo::browser;

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

} // namespace

TEST(NativeMessagingProtocolTest, EncodesAndDecodesSimpleMessage) {
    const std::string originalMessage = R"({"type":"test","value":123})";
    
    const std::string encoded = encodeNativeMessage(originalMessage);
    
    EXPECT_EQ(encoded.size(), 4 + originalMessage.size());
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded, originalMessage);
}

TEST(NativeMessagingProtocolTest, EncodesEmptyMessage) {
    const std::string originalMessage = "";
    
    const std::string encoded = encodeNativeMessage(originalMessage);
    
    EXPECT_EQ(encoded.size(), 4);
    EXPECT_EQ(encoded[0], 0);
    EXPECT_EQ(encoded[1], 0);
    EXPECT_EQ(encoded[2], 0);
    EXPECT_EQ(encoded[3], 0);
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded, "");
}

TEST(NativeMessagingProtocolTest, EncodesLargeMessage) {
    std::string originalMessage(10000, 'A');
    originalMessage = R"({"url":")" + originalMessage + R"("})";
    
    const std::string encoded = encodeNativeMessage(originalMessage);
    
    EXPECT_EQ(encoded.size(), 4 + originalMessage.size());
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded, originalMessage);
}

TEST(NativeMessagingProtocolTest, DecodesMessageWithCorrectLength) {
    const std::string message = "Hello";
    const uint32_t length = 5;
    
    std::string encoded;
    encoded.push_back(static_cast<char>(length & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 8U) & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 16U) & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 24U) & 0xFFU));
    encoded.append(message);
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded, message);
}

TEST(NativeMessagingProtocolTest, RejectsTooShortInput) {
    const std::string encoded = "ABC"; // Only 3 bytes, need at least 4
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_FALSE(success);
    EXPECT_EQ(decoded, "");
}

TEST(NativeMessagingProtocolTest, RejectsIncompleteMessage) {
    // Length says 10, but only 5 bytes of message present
    std::string encoded;
    const uint32_t length = 10;
    encoded.push_back(static_cast<char>(length & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 8U) & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 16U) & 0xFFU));
    encoded.push_back(static_cast<char>((length >> 24U) & 0xFFU));
    encoded.append("12345"); // Only 5 bytes
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_FALSE(success);
    EXPECT_EQ(decoded, "");
}

TEST(NativeMessagingProtocolTest, LittleEndianEncoding) {
    const std::string message = "X";
    const std::string encoded = encodeNativeMessage(message);
    
    // Length 1 in little-endian: 01 00 00 00
    EXPECT_EQ(static_cast<unsigned char>(encoded[0]), 0x01);
    EXPECT_EQ(static_cast<unsigned char>(encoded[1]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(encoded[2]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(encoded[3]), 0x00);
    EXPECT_EQ(encoded[4], 'X');
}

TEST(NativeMessagingProtocolTest, LargeLength256) {
    std::string message(256, 'B');
    const std::string encoded = encodeNativeMessage(message);
    
    // Length 256 (0x100) in little-endian: 00 01 00 00
    EXPECT_EQ(static_cast<unsigned char>(encoded[0]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(encoded[1]), 0x01);
    EXPECT_EQ(static_cast<unsigned char>(encoded[2]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(encoded[3]), 0x00);
    
    const auto [success, decoded] = decodeNativeMessage(encoded);
    EXPECT_TRUE(success);
    EXPECT_EQ(decoded.size(), 256);
}

TEST(NativeMessagingHostTest, ReadWriteRoundTrip) {
    // Create a stringstream to simulate stdin/stdout
    std::stringstream testStream;
    
    const std::string originalMessage = R"({"command":"addDownload","url":"https://example.com/file.zip"})";
    const std::string encoded = encodeNativeMessage(originalMessage);
    
    // Write encoded message to stream
    testStream << encoded;
    
    // Read back the length prefix
    std::array<unsigned char, 4> lengthBytes{};
    testStream.read(reinterpret_cast<char*>(lengthBytes.data()), 4);
    EXPECT_TRUE(testStream.good());
    
    const uint32_t length =
        static_cast<uint32_t>(lengthBytes[0]) |
        (static_cast<uint32_t>(lengthBytes[1]) << 8U) |
        (static_cast<uint32_t>(lengthBytes[2]) << 16U) |
        (static_cast<uint32_t>(lengthBytes[3]) << 24U);
    
    EXPECT_EQ(length, originalMessage.size());
    
    // Read the message body
    std::string message(length, '\0');
    testStream.read(message.data(), static_cast<std::streamsize>(length));
    
    EXPECT_EQ(message, originalMessage);
}

TEST(NativeMessagingHostTest, JsonMessageStructure) {
    const std::string message = R"({
        "type": "addDownload",
        "url": "https://example.com/test.zip",
        "filename": "test.zip",
        "source": "chromium"
    })";
    
    const std::string encoded = encodeNativeMessage(message);
    const auto [success, decoded] = decodeNativeMessage(encoded);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(decoded.find("\"type\"") != std::string::npos);
    EXPECT_TRUE(decoded.find("\"url\"") != std::string::npos);
    EXPECT_TRUE(decoded.find("\"filename\"") != std::string::npos);
    EXPECT_TRUE(decoded.find("https://example.com/test.zip") != std::string::npos);
}
