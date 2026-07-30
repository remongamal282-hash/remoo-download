#ifndef REMO_DOWNLOAD_UI_TRAY_ICON_H
#define REMO_DOWNLOAD_UI_TRAY_ICON_H

#include <cstdint>
#include <memory>
#include <string>

QT_BEGIN_NAMESPACE
class QSystemTrayIcon;
class QMenu;
class QAction;
QT_END_NAMESPACE

namespace remo {
namespace ui {

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    void start();
    void stop();
    void setDownloadCount(int count);
    void setTotalSpeed(double bytesPerSec);
    void showNotification(const std::string& title, const std::string& message);

private:
    bool running = false;
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_TRAY_ICON_H