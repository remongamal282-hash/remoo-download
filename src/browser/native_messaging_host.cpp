#include "browser/native_messaging_host.h"

#include <array>
#include <iostream>
#include <sstream>

namespace remo {
namespace browser {

NativeMessagingHost::NativeMessagingHost() = default;

NativeMessagingHost::~NativeMessagingHost() = default;

bool NativeMessagingHost::start() {
    running = true;
    return true;
}

void NativeMessagingHost::stop() {
    running = false;
}

bool NativeMessagingHost::isRunning() const {
    return running;
}

std::string NativeMessagingHost::readMessage() {
    std::array<unsigned char, 4> lengthBytes{};
    std::cin.read(reinterpret_cast<char*>(lengthBytes.data()), static_cast<std::streamsize>(lengthBytes.size()));
    if (std::cin.gcount() != static_cast<std::streamsize>(lengthBytes.size())) {
        return "";
    }

    const uint32_t length =
        static_cast<uint32_t>(lengthBytes[0]) |
        (static_cast<uint32_t>(lengthBytes[1]) << 8U) |
        (static_cast<uint32_t>(lengthBytes[2]) << 16U) |
        (static_cast<uint32_t>(lengthBytes[3]) << 24U);
    if (length == 0 || length > 1024 * 1024) {
        return "";
    }

    std::string message(length, '\0');
    std::cin.read(message.data(), static_cast<std::streamsize>(length));
    if (std::cin.gcount() != static_cast<std::streamsize>(length)) {
        return "";
    }
    return message;
}

bool NativeMessagingHost::writeMessage(const std::string& message) {
    if (message.size() > 1024 * 1024) {
        return false;
    }
    const uint32_t length = static_cast<uint32_t>(message.size());
    const std::array<unsigned char, 4> lengthBytes{
        static_cast<unsigned char>(length & 0xFFU),
        static_cast<unsigned char>((length >> 8U) & 0xFFU),
        static_cast<unsigned char>((length >> 16U) & 0xFFU),
        static_cast<unsigned char>((length >> 24U) & 0xFFU)
    };
    std::cout.write(reinterpret_cast<const char*>(lengthBytes.data()),
                    static_cast<std::streamsize>(lengthBytes.size()));
    std::cout.write(message.data(), static_cast<std::streamsize>(message.size()));
    std::cout.flush();
    return static_cast<bool>(std::cout);
}

} // namespace browser
} // namespace remo
