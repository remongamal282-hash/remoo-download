#ifndef REMO_DOWNLOAD_IPC_IPC_CHANNEL_H
#define REMO_DOWNLOAD_IPC_IPC_CHANNEL_H

#include <functional>
#include <memory>
#include <string>

namespace remo {
namespace ipc {

using MessageHandler = std::function<std::string(const std::string& requestJson)>;

class IIpcServer {
public:
    virtual ~IIpcServer() = default;

    virtual bool start(const std::string& channelName) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void setMessageHandler(MessageHandler handler) = 0;
};

class IIpcClient {
public:
    virtual ~IIpcClient() = default;

    virtual bool connect(const std::string& channelName, int timeoutMs = 5000) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::string sendRequest(const std::string& requestJson) = 0;
};

} // namespace ipc
} // namespace remo

#endif // REMO_DOWNLOAD_IPC_IPC_CHANNEL_H
