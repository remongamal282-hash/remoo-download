#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>
#include <QWidget>

#include "ui/main_window.h"

int main(int argc, char* argv[]) {
    // Force offscreen platform plugin if no display server is attached
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    QApplication::setApplicationName("Remoo Download");
    QApplication::setLayoutDirection(Qt::RightToLeft);

    // Load dark theme QSS
    QString themePath = QDir(QStringLiteral(REMOODOWNLOAD_SOURCE_DIR))
                            .filePath("resources/themes/dark.qss");
    QFile themeFile(themePath);
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QTextStream(&themeFile).readAll());
    }

    remo::ui::MainWindow window;
    window.show();

    // Grab the rendered window backing store into a QPixmap
    // Note: MainWindow d->window is the inner CloseInterceptWindow
    // We grab QApplication's top level widget
    QWidget* topWidget = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() || w->inherits("QMainWindow")) {
            topWidget = w;
            break;
        }
    }

    if (topWidget) {
        topWidget->resize(1000, 660);
        QPixmap pixmap = topWidget->grab();
        pixmap.save("d:/remoo-download/main_window_screenshot.png", "PNG");
    }

    return 0;
}
