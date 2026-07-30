#include "plugins/antivirus_hook.h"

namespace remo {
namespace plugins {

AntivirusHook::AntivirusHook() = default;

AntivirusHook::~AntivirusHook() = default;

std::string AntivirusHook::name() const {
    return "AntivirusHook";
}

std::string AntivirusHook::version() const {
    return "0.1.0";
}

HookType AntivirusHook::type() const {
    return HookType::AntivirusScan;
}

HookResult AntivirusHook::execute(const std::string& filePath) {
    HookResult result;
    result.success = true;
    result.filePath = filePath;
    return result;
}

bool AntivirusHook::initialize() {
    return true;
}

void AntivirusHook::shutdown() {
}

} // namespace plugins
} // namespace remo