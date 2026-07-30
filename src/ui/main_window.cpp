#include "ui/main_window.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QModelIndex>
#include <QPoint>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSplitter>
#include <QTableView>
#include <QHeaderView>
#include <QToolBar>
#include <QAction>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/add_download_dialog.h"
#include "ui/properties_dialog.h"
#include "ui/settings_dialog.h"
#include "ui/speed_graph.h"
#include "ui/tray_icon.h"

namespace remo {
namespace ui {
namespace {

QString formatSpeed(double bytesPerSec) {
    if (bytesPerSec >= 1024.0 * 1024.0) {
        return QString("%1 MB/s").arg(bytesPerSec / 1024.0 / 1024.0, 0, 'f', 1);
    }
    if (bytesPerSec >= 1024.0) {
        return QString("%1 KB/s").arg(bytesPerSec / 1024.0, 0, 'f', 1);
    }
    return QString("%1 B/s").arg(bytesPerSec, 0, 'f', 0);
}

void appendDownloadRow(QStandardItemModel* model,
                       const QString& status,
                       const QString& filename,
                       const QString& size,
                       int progress,
                       const QString& speed,
                       const QString& remaining,
                       const QString& priority,
                       const QString& category) {
    QList<QStandardItem*> row;
    for (const QString& value : {status, filename, size, QString("%1%").arg(progress),
                                 speed, remaining, priority, category}) {
        auto* item = new QStandardItem(value);
        item->setEditable(false);
        row.append(item);
    }
    model->appendRow(row);
}

} // namespace

class MainWindow::Impl {
public:
    QMainWindow* window = nullptr;
    QListWidget* sidebar = nullptr;
    QTableView* downloadTableView = nullptr;
    QStandardItemModel* downloadModel = nullptr;
    SpeedGraph* speedGraph = nullptr;
    QLabel* speedLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* spaceLabel = nullptr;
    QTimer* updateTimer = nullptr;
    TrayIcon trayIcon;
    double syntheticSpeed = 0.0;
    int tick = 0;

    ~Impl() {
        delete window;
    }
};

MainWindow::MainWindow()
    : d(std::make_unique<Impl>())
{
    d->window = new QMainWindow();
    d->window->setWindowTitle(QObject::tr("Remoo Download"));
    d->window->setLayoutDirection(Qt::RightToLeft);
    d->window->setMinimumSize(600, 400);
    d->window->resize(980, 640);

    auto* toolbar = new QToolBar(QObject::tr("الأدوات"), d->window);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d->window->addToolBar(Qt::TopToolBarArea, toolbar);

    QAction* addAction = toolbar->addAction(d->window->style()->standardIcon(QStyle::SP_FileDialogNewFolder),
                                            QObject::tr("إضافة"));
    QAction* pauseAllAction = toolbar->addAction(d->window->style()->standardIcon(QStyle::SP_MediaPause),
                                                 QObject::tr("إيقاف الكل"));
    QAction* resumeAllAction = toolbar->addAction(d->window->style()->standardIcon(QStyle::SP_MediaPlay),
                                                  QObject::tr("استئناف الكل"));
    toolbar->addSeparator();
    auto* searchEdit = new QLineEdit(d->window);
    searchEdit->setPlaceholderText(QObject::tr("بحث..."));
    searchEdit->setMaximumWidth(260);
    toolbar->addWidget(searchEdit);
    QAction* settingsAction = toolbar->addAction(d->window->style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                                                 QObject::tr("إعدادات"));

    d->sidebar = new QListWidget(d->window);
    d->sidebar->setObjectName("sidebar");
    d->sidebar->setFixedWidth(170);
    d->sidebar->addItems({QObject::tr("الكل (4)"), QObject::tr("جاري (2)"),
                          QObject::tr("متوقف (1)"), QObject::tr("مكتمل (1)"),
                          QObject::tr("فشل (0)"), QObject::tr(""),
                          QObject::tr("الفئات"), QObject::tr("فيديو (1)"),
                          QObject::tr("برامج (1)"), QObject::tr("مستندات (1)"),
                          QObject::tr("صوت (1)")});
    d->sidebar->setCurrentRow(0);

    d->downloadModel = new QStandardItemModel(d->window);
    d->downloadModel->setHorizontalHeaderLabels({QObject::tr("الحالة"), QObject::tr("اسم الملف"),
                                                 QObject::tr("الحجم"), QObject::tr("التقدم"),
                                                 QObject::tr("السرعة"), QObject::tr("الوقت المتبقي"),
                                                 QObject::tr("الأولوية"), QObject::tr("الفئة")});
    appendDownloadRow(d->downloadModel, QObject::tr("جاري"), "movie.mp4", "820 MB / 1.2 GB", 65, "3.4 MB/s", "02:14", "5", QObject::tr("فيديو"));
    appendDownloadRow(d->downloadModel, QObject::tr("متوقف"), "setup.exe", "180 MB / 450 MB", 40, "-", "--", "2", QObject::tr("برامج"));
    appendDownloadRow(d->downloadModel, QObject::tr("جاري"), "song.mp3", "7.4 MB / 8 MB", 92, "1.1 MB/s", "00:03", "3", QObject::tr("صوت"));
    appendDownloadRow(d->downloadModel, QObject::tr("مكتمل"), "report.pdf", "2 MB / 2 MB", 100, "-", "--", "0", QObject::tr("مستندات"));

    d->downloadTableView = new QTableView(d->window);
    d->downloadTableView->setModel(d->downloadModel);
    d->downloadTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->downloadTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    d->downloadTableView->setAlternatingRowColors(true);
    d->downloadTableView->setSortingEnabled(true);
    d->downloadTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    d->downloadTableView->horizontalHeader()->setStretchLastSection(true);
    d->downloadTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    d->downloadTableView->verticalHeader()->setVisible(false);

    d->speedGraph = new SpeedGraph(d->window);
    d->speedGraph->setMinimumHeight(150);

    auto* rightPane = new QWidget(d->window);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->addWidget(d->downloadTableView, 1);
    rightLayout->addWidget(d->speedGraph);

    auto* splitter = new QSplitter(Qt::Horizontal, d->window);
    splitter->addWidget(rightPane);
    splitter->addWidget(d->sidebar);
    splitter->setStretchFactor(0, 1);
    d->window->setCentralWidget(splitter);

    d->statusLabel = new QLabel(QObject::tr("نشط: 2"), d->window);
    d->speedLabel = new QLabel(QObject::tr("إجمالي السرعة: 4.5 MB/s"), d->window);
    d->spaceLabel = new QLabel(QObject::tr("المساحة المتبقية: 120 GB"), d->window);
    d->window->statusBar()->addPermanentWidget(d->statusLabel);
    d->window->statusBar()->addPermanentWidget(d->speedLabel);
    d->window->statusBar()->addPermanentWidget(d->spaceLabel);

    QObject::connect(addAction, &QAction::triggered, [this]() {
        AddDownloadDialog dialog(d->window);
        if (dialog.exec()) {
            appendDownloadRow(d->downloadModel, QObject::tr("منتظر"),
                              QString::fromStdString(dialog.filename()).isEmpty()
                                  ? QObject::tr("ملف جديد")
                                  : QString::fromStdString(dialog.filename()),
                              QObject::tr("قيد الفحص"), 0, "-", "--",
                              QString::number(dialog.priority()),
                              QString::fromStdString(dialog.category()));
            d->statusLabel->setText(QObject::tr("تمت إضافة تحميل جديد"));
        }
    });
    QObject::connect(settingsAction, &QAction::triggered, [this]() {
        SettingsDialog dialog(d->window);
        dialog.exec();
    });
    QObject::connect(d->downloadTableView, &QTableView::doubleClicked, [this](const QModelIndex& index) {
        if (index.isValid()) {
            PropertiesDialog dialog(index.row() + 1, d->window);
            dialog.exec();
        }
    });
    QObject::connect(d->downloadTableView, &QWidget::customContextMenuRequested, [this](const QPoint& point) {
        QMenu menu(d->downloadTableView);
        menu.addAction(QObject::tr("إيقاف/استئناف"));
        menu.addAction(QObject::tr("إلغاء"));
        menu.addAction(QObject::tr("فتح المجلد"));
        menu.addAction(QObject::tr("نسخ الرابط"));
        menu.addSeparator();
        menu.addAction(QObject::tr("خصائص"), [this]() {
            PropertiesDialog dialog(1, d->window);
            dialog.exec();
        });
        menu.exec(d->downloadTableView->viewport()->mapToGlobal(point));
    });
    QObject::connect(pauseAllAction, &QAction::triggered, [this]() {
        d->statusLabel->setText(QObject::tr("تم إيقاف التحميلات مؤقتًا"));
    });
    QObject::connect(resumeAllAction, &QAction::triggered, [this]() {
        d->statusLabel->setText(QObject::tr("تم استئناف التحميلات"));
    });

    d->updateTimer = new QTimer(d->window);
    QObject::connect(d->updateTimer, &QTimer::timeout, [this]() {
        ++d->tick;
        d->syntheticSpeed = (2.0 + (d->tick % 6) * 0.55) * 1024.0 * 1024.0;
        d->speedGraph->addPoint(QDateTime::currentMSecsSinceEpoch() / 1000.0, d->syntheticSpeed);
        d->speedLabel->setText(QObject::tr("إجمالي السرعة: %1").arg(formatSpeed(d->syntheticSpeed)));
        d->trayIcon.setDownloadCount(2);
        d->trayIcon.setTotalSpeed(d->syntheticSpeed);
    });
    d->updateTimer->start(1000);
}

MainWindow::~MainWindow() = default;

void MainWindow::show() {
    if (!d->window) {
        return;
    }
    d->trayIcon.start();
    d->window->show();
}

void MainWindow::close() {
    if (d->window) {
        d->window->close();
    }
}

} // namespace ui
} // namespace remo
