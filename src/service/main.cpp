#include "core/download_orchestrator.h"
#include "ipc/named_pipe_ipc.h"

#include <atomic>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int signal) {
    (void)signal;
    g_shutdownRequested = true;
}
} // namespace

int main(int argc, char* argv[]) {
    std::string dbPath = "remo_download.db";
    std::string pipeName = "remo_download_ipc";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
        } else if (arg == "--pipe" && i + 1 < argc) {
            pipeName = argv[++i];
        }
    }

    std::cout << "===========================================\n";
    std::cout << " Remoo Download Service (remo-service v0.3) \n";
    std::cout << "===========================================\n";
    std::cout << "[remo_service] Database path: " << dbPath << "\n";
    std::cout << "[remo_service] IPC Pipe name: " << pipeName << "\n";

    remo::core::DownloadOrchestrator orchestrator(dbPath);
    std::cout << "[remo_service] Initializing Core Engine & running Startup Recovery...\n";
    if (!orchestrator.start()) {
        std::cerr << "[remo_service] ERROR: Failed to start DownloadOrchestrator\n";
        return 1;
    }
    std::cout << "[remo_service] Startup Recovery completed.\n";

    remo::ipc::NamedPipeIpcServer ipcServer;
    ipcServer.setMessageHandler([&orchestrator](const std::string& requestJson) -> std::string {
        std::cout << "[remo_service IPC Request] " << requestJson << "\n";
        std::string response = orchestrator.processRequest(requestJson);
        std::cout << "[remo_service IPC Response] " << response << "\n";
        return response;
    });

    if (!ipcServer.start(pipeName)) {
        std::cerr << "[remo_service] ERROR: Failed to start Named Pipe IPC server\n";
        orchestrator.stop();
        return 1;
    }

    std::cout << "[remo_service] Service is READY and listening for IPC connections on pipe: \\\\.\\pipe\\" << pipeName << "\n";
    std::cout << "[remo_service] Press Ctrl+C to stop.\n";

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    while (!g_shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[remo_service] Shutting down service cleanly...\n";
    ipcServer.stop();
    orchestrator.stop();
    std::cout << "[remo_service] Service stopped.\n";

    return 0;
}
