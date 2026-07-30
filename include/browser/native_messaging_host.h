#ifndef REMO_DOWNLOAD_BROWSER_NATIVE_MESSAGING_HOST_H
#define REMO_DOWNLOAD_BROWSER_NATIVE_MESSAGING_HOST_H

#include <cstdint>
#include <memory>
#include <string>

namespace remo {
namespace browser {

class NativeMessagingHost {
public:
    NativeMessagingHost();
    ~NativeMessagingHost();

    bool start();
    void stop();
    bool isRunning() const;

    std::string readMessage();
    bool writeMessage(const std::string& message);

private:
    bool running = false;
};

} // namespace browser
} // namespace remo

#endif // REMO_DOWNLOAD_BROWSER_NATIVE_MESSAGING_HOST_H