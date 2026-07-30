#ifndef REMO_DOWNLOAD_IPC_NAMED_PIPE_IPC_H
#define REMO_DOWNLOAD_IPC_NAMED_PIPE_IPC_H

#include "ipc/ipc_channel.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace remo {
namespace ipc {

class NamedPipeIpcServer : public IIpcServer {
public:
    NamedPipeIpcServer();
    ~NamedPipeIpcServer() override;

    bool start(const std::string& channelName) override;
    void stop() override;
    bool isRunning() const override;
    void setMessageHandler(MessageHandler handler) override;

private:
    std::string fullPipeName;
    std::atomic<bool> running{false};
    MessageHandler messageHandler;
    std::thread listenerThread;
    mutable std::mutex mutex;

    void listenLoop();
};

class NamedPipeIpcClient : public IIpcClient {
public:
    NamedPipeIpcClient();
    ~NamedPipeIpcClient() override;

    bool connect(const std::string& channelName, int timeoutMs = 5000) override;
    void disconnect() override;
    bool isConnected() const override;
    std::string sendRequest(const std::string& requestJson) override;

private:
    std::string fullPipeName;
    void* handle = nullptr; // HANDLE (void*)
    bool connected = false;
    mutable std::mutex mutex;
};

} // namespace ipc
} // namespace remo

#endif // REMO_DOWNLOAD_IPC_NAMED_PIPE_IPC_H
