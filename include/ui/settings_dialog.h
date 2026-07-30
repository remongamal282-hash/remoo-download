#ifndef REMO_DOWNLOAD_UI_SETTINGS_DIALOG_H
#define REMO_DOWNLOAD_UI_SETTINGS_DIALOG_H

#include <cstdint>
#include <memory>
#include <string>

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QDialog;
class QWidget;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QPushButton;
class QCheckBox;
QT_END_NAMESPACE

namespace remo {
namespace ui {

class SettingsDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    int exec();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_SETTINGS_DIALOG_H
