#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <iostream>

#include "ui/main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Remo Download");
    QApplication::setApplicationVersion(REMODOWNLOAD_VERSION);
    QApplication::setOrganizationDomain(REMODOWNLOAD_DOMAIN);
    QApplication::setLayoutDirection(Qt::RightToLeft);

    QString themePath = QDir(QApplication::applicationDirPath()).filePath("resources/themes/dark.qss");
    QFile themeFile(themePath);
    if (!themeFile.exists()) {
        themeFile.setFileName(QDir(QStringLiteral(REMODOWNLOAD_SOURCE_DIR)).filePath("resources/themes/dark.qss"));
    }
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QTextStream(&themeFile).readAll());
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Remo Download - Open Source Download Manager");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption downloadOption(
        QStringList() << "d"
                      << "download",
        "Start downloading a URL immediately",
        "url");
    parser.addOption(downloadOption);

    parser.process(app);

    remo::ui::MainWindow window;
    window.show();

    if (parser.isSet(downloadOption)) {
        QString url = parser.value(downloadOption);
        Q_UNUSED(url)
    }

    return app.exec();
}
