#ifndef REMO_DOWNLOAD_PLUGINS_ANTIVIRUS_HOOK_H
#define REMO_DOWNLOAD_PLUGINS_ANTIVIRUS_HOOK_H

#include <cstdint>
#include <memory>
#include <string>

#include "plugins/plugin_interface.h"

namespace remo {
namespace plugins {

class AntivirusHook : public PluginInterface {
public:
    AntivirusHook();
    ~AntivirusHook() override;

    std::string name() const override;
    std::string version() const override;
    HookType type() const override;
    HookResult execute(const std::string& filePath) override;
    bool initialize() override;
    void shutdown() override;

    void setExecutablePath(const std::string& path);
    void setScanOnDownload(bool enabled);

private:
    std::string executablePath;
    bool scanEnabled = false;
};

} // namespace plugins
} // namespace remo

#endif // REMO_DOWNLOAD_PLUGINS_ANTIVIRUS_HOOK_H
