#ifndef REMO_DOWNLOAD_UI_PROPERTIES_DIALOG_H
#define REMO_DOWNLOAD_UI_PROPERTIES_DIALOG_H

#include <cstdint>
#include <memory>
#include <string>

QT_BEGIN_NAMESPACE
class QDialog;
class QWidget;
class QLabel;
class QProgressBar;
class QPushButton;
QT_END_NAMESPACE

namespace remo {
namespace ui {

class PropertiesDialog {
public:
    explicit PropertiesDialog(int64_t downloadId, QWidget* parent = nullptr);
    ~PropertiesDialog();

    int exec();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_PROPERTIES_DIALOG_H
