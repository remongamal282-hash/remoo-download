#ifndef REMO_DOWNLOAD_PLUGINS_PLUGIN_INTERFACE_H
#define REMO_DOWNLOAD_PLUGINS_PLUGIN_INTERFACE_H

#include <cstdint>
#include <memory>
#include <string>

namespace remo {
namespace plugins {

enum class HookType {
    AntivirusScan,
    PreDownload,
    PostDownload,
    Custom
};

struct HookResult {
    bool success = true;
    std::string message;
    std::string filePath;
};

class PluginInterface {
public:
    virtual ~PluginInterface() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual HookType type() const = 0;
    virtual HookResult execute(const std::string& filePath) = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
};

} // namespace plugins
} // namespace remo

#endif // REMO_DOWNLOAD_PLUGINS_PLUGIN_INTERFACE_H