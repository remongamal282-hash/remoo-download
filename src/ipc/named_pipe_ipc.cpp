#include "ipc/named_pipe_ipc.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace remo {
namespace ipc {

static std::string formatPipeName(const std::string& channelName) {
#ifdef _WIN32
    if (channelName.rfind("\\\\.\\pipe\\", 0) == 0) {
        return channelName;
    }
    return "\\\\.\\pipe\\" + channelName;
#else
    return channelName;
#endif
}

NamedPipeIpcServer::NamedPipeIpcServer() = default;

NamedPipeIpcServer::~NamedPipeIpcServer() {
    stop();
}

bool NamedPipeIpcServer::start(const std::string& channelName) {
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        return true;
    }

    fullPipeName = formatPipeName(channelName);
    running = true;
    listenerThread = std::thread([this]() { listenLoop(); });
    return true;
}

void NamedPipeIpcServer::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!running) {
            return;
        }
        running = false;
    }

#ifdef _WIN32
    // Connect a dummy client to wake up blocking ConnectNamedPipe
    HANDLE hPipe = CreateFileA(
        fullPipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
    }
#endif

    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

bool NamedPipeIpcServer::isRunning() const {
    return running;
}

void NamedPipeIpcServer::setMessageHandler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(mutex);
    messageHandler = handler;
}

void NamedPipeIpcServer::listenLoop() {
#ifdef _WIN32
    constexpr DWORD kBufferSize = 65536;

    while (running) {
        HANDLE hPipe = CreateNamedPipeA(
            fullPipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            kBufferSize,
            kBufferSize,
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!running) {
            CloseHandle(hPipe);
            break;
        }

        if (connected) {
            std::vector<char> buffer(kBufferSize);
            DWORD bytesRead = 0;
            BOOL readSuccess = ReadFile(hPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, NULL);

            if (readSuccess && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string requestJson(buffer.data(), bytesRead);

                MessageHandler handlerCopy;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    handlerCopy = messageHandler;
                }

                std::string responseJson = "{\"success\":false,\"error\":\"No handler set\"}";
                if (handlerCopy) {
                    responseJson = handlerCopy(requestJson);
                }

                DWORD bytesWritten = 0;
                WriteFile(hPipe, responseJson.c_str(), static_cast<DWORD>(responseJson.size()), &bytesWritten, NULL);
                FlushFileBuffers(hPipe);
            }
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
#else
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#endif
}

NamedPipeIpcClient::NamedPipeIpcClient() = default;

NamedPipeIpcClient::~NamedPipeIpcClient() {
    disconnect();
}

bool NamedPipeIpcClient::connect(const std::string& channelName, int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex);
    fullPipeName = formatPipeName(channelName);

#ifdef _WIN32
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (WaitNamedPipeA(fullPipeName.c_str(), static_cast<DWORD>(timeoutMs))) {
            connected = true;
            return true;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
#else
    (void)timeoutMs;
    connected = true;
    return true;
#endif
}

void NamedPipeIpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex);
    connected = false;
}

bool NamedPipeIpcClient::isConnected() const {
    return connected;
}

std::string NamedPipeIpcClient::sendRequest(const std::string& requestJson) {
    std::lock_guard<std::mutex> lock(mutex);

#ifdef _WIN32
    HANDLE hPipe = CreateFileA(
        fullPipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        if (WaitNamedPipeA(fullPipeName.c_str(), 2000)) {
            hPipe = CreateFileA(
                fullPipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        return "{\"success\":false,\"errorMessage\":\"Failed to connect to Named Pipe\"}";
    }

    DWORD dwMode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);

    DWORD bytesWritten = 0;
    BOOL writeSuccess = WriteFile(
        hPipe,
        requestJson.c_str(),
        static_cast<DWORD>(requestJson.size()),
        &bytesWritten,
        NULL
    );

    if (!writeSuccess) {
        CloseHandle(hPipe);
        return "{\"success\":false,\"errorMessage\":\"Failed to write to pipe\"}";
    }

    constexpr DWORD kBufferSize = 65536;
    std::vector<char> buffer(kBufferSize);
    DWORD bytesRead = 0;
    BOOL readSuccess = ReadFile(
        hPipe,
        buffer.data(),
        static_cast<DWORD>(buffer.size() - 1),
        &bytesRead,
        NULL
    );

    CloseHandle(hPipe);

    if (!readSuccess || bytesRead == 0) {
        return "{\"success\":false,\"errorMessage\":\"Failed to read from pipe\"}";
    }

    buffer[bytesRead] = '\0';
    return std::string(buffer.data(), bytesRead);
#else
    (void)requestJson;
    return "{\"success\":true}";
#endif
}

} // namespace ipc
} // namespace remo
