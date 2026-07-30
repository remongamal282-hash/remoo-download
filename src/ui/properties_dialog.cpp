#include "ui/properties_dialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "ui/speed_graph.h"

namespace remo {
namespace ui {

class PropertiesDialog::Impl {
public:
    QDialog* dialog = nullptr;

    ~Impl() {
        delete dialog;
    }
};

PropertiesDialog::PropertiesDialog(int64_t downloadId, QWidget* parent)
    : d(std::make_unique<Impl>())
{
    d->dialog = new QDialog(parent);
    d->dialog->setWindowTitle(QObject::tr("خصائص التحميل #%1").arg(downloadId));
    d->dialog->setLayoutDirection(Qt::RightToLeft);
    d->dialog->setMinimumSize(520, 420);

    auto* tabs = new QTabWidget(d->dialog);

    auto* generalPage = new QWidget(tabs);
    auto* generalLayout = new QFormLayout(generalPage);
    auto* progress = new QProgressBar(generalPage);
    progress->setRange(0, 100);
    progress->setValue(65);
    generalLayout->addRow(QObject::tr("الرابط:"), new QLabel(QObject::tr("غير متصل بمحرك التحميل بعد"), generalPage));
    generalLayout->addRow(QObject::tr("الحجم الكلي:"), new QLabel(QObject::tr("قيد الفحص"), generalPage));
    generalLayout->addRow(QObject::tr("المحمّل:"), progress);
    generalLayout->addRow(QObject::tr("السرعة:"), new QLabel(QObject::tr("0 B/s"), generalPage));
    generalLayout->addRow(QObject::tr("الاتصالات النشطة:"), new QLabel(QObject::tr("0/4"), generalPage));
    generalLayout->addRow(QObject::tr("الحالة:"), new QLabel(QObject::tr("منتظر"), generalPage));

    auto* performancePage = new QWidget(tabs);
    auto* performanceLayout = new QVBoxLayout(performancePage);
    auto* graph = new SpeedGraph(performancePage);
    graph->setMinimumHeight(180);
    performanceLayout->addWidget(graph);
    auto* exportHint = new QLabel(QObject::tr("تصدير CSV سيتصل ببيانات الأداء عند دمج محرك التحميل."), performancePage);
    exportHint->setObjectName("secondaryText");
    performanceLayout->addWidget(exportHint);

    auto* logPage = new QWidget(tabs);
    auto* logLayout = new QVBoxLayout(logPage);
    auto* eventsTable = new QTableWidget(0, 3, logPage);
    eventsTable->setHorizontalHeaderLabels({QObject::tr("الوقت"), QObject::tr("الحدث"), QObject::tr("التفاصيل")});
    eventsTable->setAlternatingRowColors(true);
    logLayout->addWidget(eventsTable);

    auto* integrityPage = new QWidget(tabs);
    auto* integrityLayout = new QFormLayout(integrityPage);
    integrityLayout->addRow(QObject::tr("Checksum:"), new QLabel(QObject::tr("لم يُحسب بعد"), integrityPage));
    integrityLayout->addRow(QObject::tr("النتيجة:"), new QLabel(QObject::tr("غير متاح"), integrityPage));

    tabs->addTab(generalPage, QObject::tr("عام"));
    tabs->addTab(performancePage, QObject::tr("الأداء"));
    tabs->addTab(logPage, QObject::tr("السجل"));
    tabs->addTab(integrityPage, QObject::tr("السلامة"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, d->dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QObject::tr("إغلاق"));
    QObject::connect(buttons, &QDialogButtonBox::rejected, d->dialog, &QDialog::reject);

    auto* layout = new QVBoxLayout(d->dialog);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

PropertiesDialog::~PropertiesDialog() = default;

int PropertiesDialog::exec() {
    return d->dialog ? d->dialog->exec() : QDialog::Rejected;
}

} // namespace ui
} // namespace remo
