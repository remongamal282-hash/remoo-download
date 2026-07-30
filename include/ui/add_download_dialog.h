#ifndef REMO_DOWNLOAD_UI_ADD_DOWNLOAD_DIALOG_H
#define REMO_DOWNLOAD_UI_ADD_DOWNLOAD_DIALOG_H

#include <cstdint>
#include <memory>
#include <string>

QT_BEGIN_NAMESPACE
class QDialog;
class QWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
QT_END_NAMESPACE

namespace remo {
namespace ui {

class AddDownloadDialog {
public:
    explicit AddDownloadDialog(QWidget* parent = nullptr);
    ~AddDownloadDialog();

    std::string url() const;
    std::string filename() const;
    std::string savePath() const;
    int maxConnections() const;
    int priority() const;
    std::string category() const;
    bool exec();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_ADD_DOWNLOAD_DIALOG_H
