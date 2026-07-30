#ifndef REMO_DOWNLOAD_UI_MAIN_WINDOW_H
#define REMO_DOWNLOAD_UI_MAIN_WINDOW_H

#include <cstdint>
#include <memory>
#include <string>

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QMainWindow;
class QTableView;
class QProgressBar;
class QLabel;
class QPushButton;
class QTimer;
QT_END_NAMESPACE

namespace remo {
namespace ui {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    void show();
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_MAIN_WINDOW_H
