#include "browser/native_messaging_host.h"

#include <iostream>
#include <string>

int main() {
    remo::browser::NativeMessagingHost host;
    if (!host.start()) {
        return 1;
    }

    while (host.isRunning()) {
        const std::string message = host.readMessage();
        if (message.empty()) {
            break;
        }

        const bool looksLikeAddDownload =
            message.find("\"addDownload\"") != std::string::npos &&
            message.find("\"url\"") != std::string::npos;

        const std::string response = looksLikeAddDownload
            ? R"({"ok":true,"status":"accepted"})"
            : R"({"ok":false,"error":"unsupported_message"})";

        if (!host.writeMessage(response)) {
            break;
        }
    }

    host.stop();
    return 0;
}
