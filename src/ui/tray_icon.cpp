#include "ui/tray_icon.h"

#include <QSystemTrayIcon>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QStyle>

namespace remo {
namespace ui {

class TrayIcon::Impl {
public:
    QSystemTrayIcon* trayIcon = nullptr;
    QMenu* trayMenu = nullptr;
    QAction* openAction = nullptr;
    QAction* pauseAllAction = nullptr;
    QAction* resumeAllAction = nullptr;
    QAction* openFolderAction = nullptr;
    QAction* settingsAction = nullptr;
    QAction* exitAction = nullptr;
    int downloadCount = 0;
    double totalSpeed = 0.0;

    QString tooltip() const {
        return QObject::tr("%1 تحميلات نشطة - %2 MB/s")
            .arg(downloadCount)
            .arg(totalSpeed / 1024.0 / 1024.0, 0, 'f', 2);
    }

    ~Impl() {
        delete trayIcon;
        delete trayMenu;
    }
};

TrayIcon::TrayIcon()
    : d(std::make_unique<Impl>())
{
    d->trayMenu = new QMenu();
    d->openAction = d->trayMenu->addAction(QObject::tr("فتح Remo Download"));
    d->pauseAllAction = d->trayMenu->addAction(QObject::tr("إيقاف الكل"));
    d->resumeAllAction = d->trayMenu->addAction(QObject::tr("استئناف الكل"));
    d->trayMenu->addSeparator();
    d->openFolderAction = d->trayMenu->addAction(QObject::tr("فتح مجلد التحميلات"));
    d->settingsAction = d->trayMenu->addAction(QObject::tr("إعدادات"));
    d->trayMenu->addSeparator();
    d->exitAction = d->trayMenu->addAction(QObject::tr("خروج"));

    d->trayIcon = new QSystemTrayIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown), nullptr);
    d->trayIcon->setContextMenu(d->trayMenu);
    d->trayIcon->setToolTip(d->tooltip());

    QObject::connect(d->exitAction, &QAction::triggered, qApp, &QApplication::quit);
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::start() {
    if (d->trayIcon && QSystemTrayIcon::isSystemTrayAvailable()) {
        d->trayIcon->show();
        running = true;
    }
}

void TrayIcon::stop() {
    if (d->trayIcon) {
        d->trayIcon->hide();
    }
    running = false;
}

void TrayIcon::setDownloadCount(int count) {
    d->downloadCount = count;
    if (d->trayIcon) {
        d->trayIcon->setToolTip(d->tooltip());
    }
}

void TrayIcon::setTotalSpeed(double bytesPerSec) {
    d->totalSpeed = bytesPerSec;
    if (d->trayIcon) {
        d->trayIcon->setToolTip(d->tooltip());
    }
}

void TrayIcon::showNotification(const std::string& title, const std::string& message) {
    if (d->trayIcon && running) {
        d->trayIcon->showMessage(QString::fromStdString(title), QString::fromStdString(message),
                                 QSystemTrayIcon::Information, 4000);
    }
}

} // namespace ui
} // namespace remo
