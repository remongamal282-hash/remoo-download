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
#include <QProgressBar>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QHeaderView>
#include <QToolBar>
#include <QAction>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>
#include <QStandardPaths>

#include "ui/add_download_dialog.h"
#include "ui/ipc_service_client.h"
#include "ui/properties_dialog.h"
#include "ui/settings_dialog.h"
#include "ui/speed_graph.h"
#include "ui/tray_icon.h"

namespace remo {
namespace ui {

// ---------------------------------------------------------------------------
// Column indices
// ---------------------------------------------------------------------------
enum Col {
    ColStatus   = 0,
    ColFilename = 1,
    ColSize     = 2,
    ColProgress = 3,
    ColSpeed    = 4,
    ColCategory = 5,
    ColCount    = 6
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

QString formatBytes(int64_t bytes) {
    if (bytes <= 0) return QObject::tr("غير معروف");
    if (bytes >= int64_t(1024) * 1024 * 1024)
        return QString("%1 GB").arg(bytes / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2);
    if (bytes >= 1024 * 1024)
        return QString("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytes >= 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 B").arg(bytes);
}

QString formatSpeed(double bytesPerSec) {
    if (bytesPerSec >= 1024.0 * 1024.0)
        return QString("%1 MB/s").arg(bytesPerSec / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytesPerSec >= 1024.0)
        return QString("%1 KB/s").arg(bytesPerSec / 1024.0, 0, 'f', 1);
    if (bytesPerSec > 0)
        return QString("%1 B/s").arg(bytesPerSec, 0, 'f', 0);
    return QString("-");
}

QString statusAr(const std::string& s) {
    if (s == "downloading")  return QObject::tr("جاري");
    if (s == "paused")       return QObject::tr("متوقف");
    if (s == "completed")    return QObject::tr("مكتمل");
    if (s == "failed")       return QObject::tr("فشل");
    if (s == "cancelled")    return QObject::tr("ملغي");
    if (s == "queued")       return QObject::tr("منتظر");
    return QObject::tr("غير معروف");
}

} // namespace

// ---------------------------------------------------------------------------
// ProgressDelegate — renders a QProgressBar inside the progress column
// ---------------------------------------------------------------------------
class ProgressDelegate : public QStyledItemDelegate {
public:
    explicit ProgressDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        bool ok = false;
        int val = index.data(Qt::DisplayRole).toString().replace('%', "").toInt(&ok);
        if (!ok) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionProgressBar pbOpt;
        pbOpt.rect    = option.rect.adjusted(4, 4, -4, -4);
        pbOpt.minimum = 0;
        pbOpt.maximum = 100;
        pbOpt.progress = val;
        pbOpt.text = QString("%1%").arg(val);
        pbOpt.textVisible = true;
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pbOpt, painter);
    }
};

// ---------------------------------------------------------------------------
// CloseInterceptWindow — QMainWindow that minimises to tray on close
// ---------------------------------------------------------------------------
class CloseInterceptWindow : public QMainWindow {
public:
    explicit CloseInterceptWindow(MainWindow* owner, QWidget* parent = nullptr)
        : QMainWindow(parent), m_owner(owner) {}

protected:
    void closeEvent(QCloseEvent* ev) override {
        if (m_owner && m_owner->handleCloseEvent()) {
            ev->ignore(); // swallow — minimise to tray handled in handleCloseEvent
        } else {
            QMainWindow::closeEvent(ev);
        }
    }
private:
    MainWindow* m_owner = nullptr;
};

// ---------------------------------------------------------------------------
// MainWindow::Impl
// ---------------------------------------------------------------------------
class MainWindow::Impl {
public:
    CloseInterceptWindow* window  = nullptr;
    QListWidget*          sidebar = nullptr;
    QTableView*           table   = nullptr;
    QStandardItemModel*   model   = nullptr;
    SpeedGraph*           graph   = nullptr;
    QLabel*               connLabel = nullptr;
    QLabel*               speedLabel = nullptr;
    QLabel*               spaceLabel = nullptr;
    QTimer*               pollTimer  = nullptr;
    TrayIcon              tray;

    IpcServiceClient      ipcClient;
    bool                  serviceConnected = false;

    // Map: downloadId → row index in model (for fast update)
    // We simply rebuild the model each poll for simplicity in v0.4
    std::vector<int> rowDownloadIds; // rowDownloadIds[row] = downloadId

    ~Impl() { delete window; }

    void updateConnectionStatus(bool connected) {
        serviceConnected = connected;
        if (connLabel) {
            if (connected) {
                connLabel->setText(QObject::tr("● متصل بالخدمة"));
                connLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
            } else {
                connLabel->setText(QObject::tr("● غير متصل بالخدمة"));
                connLabel->setStyleSheet("color: #F44336; font-weight: bold;");
            }
        }
    }

    void rebuildModel(const std::vector<DownloadStatusInfo>& downloads) {
        rowDownloadIds.clear();
        model->removeRows(0, model->rowCount());

        int activeCount = 0;
        double totalSpeed = 0.0;

        for (const auto& dl : downloads) {
            QList<QStandardItem*> row;

            auto* colStatus   = new QStandardItem(statusAr(dl.status));
            auto* colFilename = new QStandardItem(QString::fromStdString(
                dl.filename.empty() ? dl.url : dl.filename));
            auto* colSize     = new QStandardItem(
                formatBytes(dl.totalBytes) + " / " + formatBytes(dl.downloadedBytes));
            auto* colProgress = new QStandardItem(
                QString("%1%").arg(static_cast<int>(dl.progressPct)));
            auto* colSpeed    = new QStandardItem(formatSpeed(dl.speedBytesPerSec));
            auto* colCat      = new QStandardItem(QString("-"));

            colStatus->setData(dl.id, Qt::UserRole);   // store downloadId

            for (auto* item : {colStatus, colFilename, colSize, colProgress, colSpeed, colCat})
                item->setEditable(false);

            // Colour-code status
            if (dl.status == "downloading") {
                colStatus->setForeground(QColor("#4CAF50"));
                ++activeCount;
                totalSpeed += dl.speedBytesPerSec;
            } else if (dl.status == "paused") {
                colStatus->setForeground(QColor("#FFC107"));
            } else if (dl.status == "failed" || dl.status == "cancelled") {
                colStatus->setForeground(QColor("#F44336"));
            } else if (dl.status == "completed") {
                colStatus->setForeground(QColor("#2196F3"));
            }

            row << colStatus << colFilename << colSize << colProgress << colSpeed << colCat;
            model->appendRow(row);
            rowDownloadIds.push_back(dl.id);
        }

        // Update status bar
        if (speedLabel)
            speedLabel->setText(QObject::tr("إجمالي السرعة: %1").arg(formatSpeed(totalSpeed)));

        // Update graph
        if (graph)
            graph->addPoint(QDateTime::currentMSecsSinceEpoch() / 1000.0, totalSpeed);

        // Update tray
        tray.setDownloadCount(activeCount);
        tray.setTotalSpeed(totalSpeed);
    }

    int selectedDownloadId() const {
        if (!table) return 0;
        auto sel = table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return 0;
        int row = sel.first().row();
        if (row < 0 || row >= static_cast<int>(rowDownloadIds.size())) return 0;
        return rowDownloadIds[row];
    }
};

// ---------------------------------------------------------------------------
// MainWindow — constructor
// ---------------------------------------------------------------------------
MainWindow::MainWindow()
    : d(std::make_unique<Impl>())
{
    d->window = new CloseInterceptWindow(this);
    d->window->setWindowTitle(QObject::tr("Remoo Download"));
    d->window->setLayoutDirection(Qt::RightToLeft);
    d->window->setMinimumSize(700, 450);
    d->window->resize(1000, 660);

    // ------------------------------------------------------------------
    // Toolbar
    // ------------------------------------------------------------------
    auto* toolbar = new QToolBar(QObject::tr("الأدوات"), d->window);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    d->window->addToolBar(Qt::TopToolBarArea, toolbar);

    QAction* addAction = toolbar->addAction(
        d->window->style()->standardIcon(QStyle::SP_FileDialogNewFolder),
        QObject::tr("إضافة تحميل"));
    addAction->setObjectName("toolAdd");

    QAction* pauseAllAction = toolbar->addAction(
        d->window->style()->standardIcon(QStyle::SP_MediaPause),
        QObject::tr("إيقاف الكل"));
    pauseAllAction->setObjectName("toolPauseAll");

    QAction* resumeAllAction = toolbar->addAction(
        d->window->style()->standardIcon(QStyle::SP_MediaPlay),
        QObject::tr("استئناف الكل"));
    resumeAllAction->setObjectName("toolResumeAll");

    toolbar->addSeparator();

    auto* searchEdit = new QLineEdit(d->window);
    searchEdit->setPlaceholderText(QObject::tr("بحث..."));
    searchEdit->setMaximumWidth(260);
    toolbar->addWidget(searchEdit);

    QAction* settingsAction = toolbar->addAction(
        d->window->style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        QObject::tr("إعدادات"));

    // ------------------------------------------------------------------
    // Sidebar
    // ------------------------------------------------------------------
    d->sidebar = new QListWidget(d->window);
    d->sidebar->setObjectName("sidebar");
    d->sidebar->setFixedWidth(170);
    d->sidebar->addItems({
        QObject::tr("الكل"),
        QObject::tr("جاري"),
        QObject::tr("متوقف"),
        QObject::tr("مكتمل"),
        QObject::tr("فشل"),
        QObject::tr(""),
        QObject::tr("الفئات"),
        QObject::tr("فيديو"),
        QObject::tr("برامج"),
        QObject::tr("مستندات"),
        QObject::tr("صوت")
    });
    d->sidebar->setCurrentRow(0);

    // ------------------------------------------------------------------
    // Download table
    // ------------------------------------------------------------------
    d->model = new QStandardItemModel(d->window);
    d->model->setHorizontalHeaderLabels({
        QObject::tr("الحالة"),
        QObject::tr("اسم الملف"),
        QObject::tr("الحجم"),
        QObject::tr("التقدم"),
        QObject::tr("السرعة"),
        QObject::tr("الفئة")
    });

    d->table = new QTableView(d->window);
    d->table->setModel(d->model);
    d->table->setItemDelegateForColumn(ColProgress, new ProgressDelegate(d->table));
    d->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->table->setSelectionMode(QAbstractItemView::SingleSelection);
    d->table->setAlternatingRowColors(true);
    d->table->setSortingEnabled(true);
    d->table->setContextMenuPolicy(Qt::CustomContextMenu);
    d->table->horizontalHeader()->setStretchLastSection(true);
    d->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    d->table->horizontalHeader()->setSectionResizeMode(ColFilename, QHeaderView::Stretch);
    d->table->verticalHeader()->setVisible(false);
    d->table->setObjectName("downloadTable");
    d->table->setColumnWidth(ColStatus,   90);
    d->table->setColumnWidth(ColSize,    140);
    d->table->setColumnWidth(ColProgress, 110);
    d->table->setColumnWidth(ColSpeed,    100);

    // ------------------------------------------------------------------
    // Speed graph
    // ------------------------------------------------------------------
    d->graph = new SpeedGraph(d->window);
    d->graph->setMinimumHeight(130);
    d->graph->setMaximumHeight(160);

    auto* rightPane   = new QWidget(d->window);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(6);
    rightLayout->addWidget(d->table, 1);
    rightLayout->addWidget(d->graph);

    auto* splitter = new QSplitter(Qt::Horizontal, d->window);
    splitter->addWidget(rightPane);
    splitter->addWidget(d->sidebar);
    splitter->setStretchFactor(0, 1);
    d->window->setCentralWidget(splitter);

    // ------------------------------------------------------------------
    // Status bar
    // ------------------------------------------------------------------
    d->connLabel  = new QLabel(d->window);
    d->speedLabel = new QLabel(QObject::tr("إجمالي السرعة: -"), d->window);
    d->spaceLabel = new QLabel(QObject::tr("جاري الاتصال..."), d->window);

    d->window->statusBar()->addPermanentWidget(d->connLabel);
    d->window->statusBar()->addPermanentWidget(d->speedLabel);
    d->window->statusBar()->addPermanentWidget(d->spaceLabel);

    d->updateConnectionStatus(false);

    // ------------------------------------------------------------------
    // Signals
    // ------------------------------------------------------------------

    // Add download
    QObject::connect(addAction, &QAction::triggered, [this]() {
        if (!d->serviceConnected) {
            QMessageBox::warning(d->window,
                QObject::tr("غير متصل"),
                QObject::tr("لا يمكن إضافة تحميل: الخدمة غير متصلة.\n"
                            "تأكد من تشغيل remo_service ثم أعد المحاولة."));
            return;
        }
        AddDownloadDialog dlg(d->window, &d->ipcClient);
        if (dlg.exec()) {
            d->spaceLabel->setText(QObject::tr("تمت إضافة التحميل — جاري التحديث..."));
        }
    });

    // Settings
    QObject::connect(settingsAction, &QAction::triggered, [this]() {
        SettingsDialog dlg(d->window);
        dlg.exec();
    });

    // Double-click → properties
    QObject::connect(d->table, &QTableView::doubleClicked,
                     [this](const QModelIndex& idx) {
        if (!idx.isValid()) return;
        int id = d->selectedDownloadId();
        if (id > 0) {
            PropertiesDialog dlg(id, d->window);
            dlg.exec();
        }
    });

    // Right-click context menu
    QObject::connect(d->table, &QWidget::customContextMenuRequested,
                     [this](const QPoint& pt) {
        int downloadId = d->selectedDownloadId();
        if (downloadId <= 0) return;

        QMenu menu(d->table);

        auto* pauseAct  = menu.addAction(QObject::tr("إيقاف"));
        auto* resumeAct = menu.addAction(QObject::tr("استئناف"));
        auto* cancelAct = menu.addAction(QObject::tr("إلغاء"));
        menu.addSeparator();
        auto* propsAct  = menu.addAction(QObject::tr("خصائص"));

        QObject::connect(pauseAct, &QAction::triggered, [this, downloadId]() {
            if (d->ipcClient.pauseDownload(downloadId)) {
                d->spaceLabel->setText(QObject::tr("تم إيقاف التحميل #%1").arg(downloadId));
            }
        });
        QObject::connect(resumeAct, &QAction::triggered, [this, downloadId]() {
            if (d->ipcClient.resumeDownload(downloadId)) {
                d->spaceLabel->setText(QObject::tr("تم استئناف التحميل #%1").arg(downloadId));
            }
        });
        QObject::connect(cancelAct, &QAction::triggered, [this, downloadId]() {
            auto btn = QMessageBox::question(d->window,
                QObject::tr("تأكيد الإلغاء"),
                QObject::tr("هل تريد إلغاء التحميل #%1 نهائياً؟").arg(downloadId));
            if (btn == QMessageBox::Yes) {
                d->ipcClient.cancelDownload(downloadId);
                d->spaceLabel->setText(QObject::tr("تم إلغاء التحميل #%1").arg(downloadId));
            }
        });
        QObject::connect(propsAct, &QAction::triggered, [this, downloadId]() {
            PropertiesDialog dlg(downloadId, d->window);
            dlg.exec();
        });

        menu.exec(d->table->viewport()->mapToGlobal(pt));
    });

    // Pause all / Resume all (fire IPC for each known download)
    QObject::connect(pauseAllAction, &QAction::triggered, [this]() {
        for (int id : d->rowDownloadIds) {
            d->ipcClient.pauseDownload(id);
        }
        d->spaceLabel->setText(QObject::tr("تم إيقاف كل التحميلات"));
    });
    QObject::connect(resumeAllAction, &QAction::triggered, [this]() {
        for (int id : d->rowDownloadIds) {
            d->ipcClient.resumeDownload(id);
        }
        d->spaceLabel->setText(QObject::tr("تم استئناف كل التحميلات"));
    });

    // ------------------------------------------------------------------
    // Polling timer — every 1 second
    // ------------------------------------------------------------------
    d->pollTimer = new QTimer(d->window);
    QObject::connect(d->pollTimer, &QTimer::timeout, [this]() {
        // Try (re)connect if not connected
        if (!d->serviceConnected) {
            d->ipcClient.connectToService("remo_download_ipc", 300);
        }

        auto downloads = d->ipcClient.getStatus(0); // 0 = all
        bool nowConnected = d->ipcClient.isConnected();

        if (nowConnected != d->serviceConnected) {
            d->updateConnectionStatus(nowConnected);
        }

        if (nowConnected) {
            d->rebuildModel(downloads);
        }
    });
    d->pollTimer->start(1000);
}

// ---------------------------------------------------------------------------
MainWindow::~MainWindow() = default;

void MainWindow::show() {
    if (!d->window) return;
    d->tray.start(d->window); // pass window pointer so open-action can show it
    d->window->show();
}

void MainWindow::close() {
    if (d->window) d->window->close();
}

bool MainWindow::handleCloseEvent() {
    // Minimise to tray instead of closing
    d->window->hide();
    d->tray.showNotification("Remoo Download",
        "التطبيق يعمل في الخلفية. اضغط على الأيقونة لفتحه مجدداً.");
    return true; // event consumed
}

} // namespace ui
} // namespace remo
