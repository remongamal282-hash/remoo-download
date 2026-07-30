#include "ui/settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <utility>

namespace remo {
namespace ui {

class SettingsDialog::Impl {
public:
    QDialog* dialog = nullptr;

    ~Impl() {
        delete dialog;
    }
};

SettingsDialog::SettingsDialog(QWidget* parent)
    : d(std::make_unique<Impl>())
{
    d->dialog = new QDialog(parent);
    d->dialog->setWindowTitle(QObject::tr("الإعدادات"));
    d->dialog->setLayoutDirection(Qt::RightToLeft);
    d->dialog->setMinimumSize(700, 460);

    auto* categories = new QListWidget(d->dialog);
    categories->setFixedWidth(150);
    categories->addItems({QObject::tr("عام"), QObject::tr("التحميل"), QObject::tr("السرعة"),
                          QObject::tr("الفئات"), QObject::tr("الجدولة"), QObject::tr("المتصفح"),
                          QObject::tr("الإشعارات"), QObject::tr("المظهر"), QObject::tr("متقدم")});

    auto* pages = new QStackedWidget(d->dialog);

    auto makePage = [](QWidget* parent) {
        auto* page = new QWidget(parent);
        auto* form = new QFormLayout(page);
        form->setLabelAlignment(Qt::AlignRight);
        return std::pair<QWidget*, QFormLayout*>(page, form);
    };

    auto [generalPage, generalForm] = makePage(pages);
    auto* languageCombo = new QComboBox(generalPage);
    languageCombo->addItems({QObject::tr("العربية"), QObject::tr("English")});
    generalForm->addRow(QObject::tr("اللغة:"), languageCombo);
    generalForm->addRow(QObject::tr("بدء التشغيل مع النظام:"), new QCheckBox(generalPage));
    auto* closeBehaviorCombo = new QComboBox(generalPage);
    closeBehaviorCombo->addItems({QObject::tr("تصغير إلى Tray"), QObject::tr("إغلاق فعلي")});
    generalForm->addRow(QObject::tr("عند الإغلاق:"), closeBehaviorCombo);

    auto [downloadPage, downloadForm] = makePage(pages);
    auto* concurrentSpin = new QSpinBox(downloadPage);
    concurrentSpin->setRange(1, 32);
    concurrentSpin->setValue(3);
    auto* connectionsSpin = new QSpinBox(downloadPage);
    connectionsSpin->setRange(1, 32);
    connectionsSpin->setValue(4);
    downloadForm->addRow(QObject::tr("التحميلات المتزامنة:"), concurrentSpin);
    downloadForm->addRow(QObject::tr("اتصالات لكل ملف:"), connectionsSpin);

    auto [speedPage, speedForm] = makePage(pages);
    auto* speedLimitSpin = new QSpinBox(speedPage);
    speedLimitSpin->setRange(0, 100000);
    speedLimitSpin->setSuffix(QObject::tr(" KB/s"));
    speedForm->addRow(QObject::tr("الحد العام:"), speedLimitSpin);
    speedForm->addRow(QObject::tr("جدولة السرعة:"), new QCheckBox(speedPage));

    auto [categoriesPage, categoriesForm] = makePage(pages);
    categoriesForm->addRow(QObject::tr("إدارة الفئات:"), new QPushButton(QObject::tr("فتح مدير الفئات"), categoriesPage));

    auto [schedulerPage, schedulerForm] = makePage(pages);
    schedulerForm->addRow(QObject::tr("وضع الهدوء:"), new QCheckBox(schedulerPage));

    auto [browserPage, browserForm] = makePage(pages);
    browserForm->addRow(QObject::tr("اعتراض التحميلات:"), new QCheckBox(browserPage));
    browserForm->addRow(QObject::tr("مراقبة الحافظة:"), new QCheckBox(browserPage));

    auto [notificationsPage, notificationsForm] = makePage(pages);
    notificationsForm->addRow(QObject::tr("إشعارات النظام:"), new QCheckBox(notificationsPage));
    notificationsForm->addRow(QObject::tr("الصوت:"), new QCheckBox(notificationsPage));

    auto [appearancePage, appearanceForm] = makePage(pages);
    auto* themeCombo = new QComboBox(appearancePage);
    themeCombo->addItems({QObject::tr("حسب النظام"), QObject::tr("داكن"), QObject::tr("فاتح"), QObject::tr("ملف QSS مخصص")});
    auto* fontSpin = new QSpinBox(appearancePage);
    fontSpin->setRange(10, 24);
    fontSpin->setValue(14);
    appearanceForm->addRow(QObject::tr("الثيم:"), themeCombo);
    appearanceForm->addRow(QObject::tr("حجم الخط:"), fontSpin);

    auto [advancedPage, advancedForm] = makePage(pages);
    advancedForm->addRow(QObject::tr("مسار قاعدة البيانات:"), new QLineEdit(advancedPage));
    auto* logLevelCombo = new QComboBox(advancedPage);
    logLevelCombo->addItems({QObject::tr("Info"), QObject::tr("Debug"), QObject::tr("Warning")});
    advancedForm->addRow(QObject::tr("مستوى السجلات:"), logLevelCombo);

    for (QWidget* page : {generalPage, downloadPage, speedPage, categoriesPage, schedulerPage,
                          browserPage, notificationsPage, appearancePage, advancedPage}) {
        pages->addWidget(page);
    }

    QObject::connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    categories->setCurrentRow(0);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, d->dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("حفظ"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("إلغاء"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, d->dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, d->dialog, &QDialog::reject);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->addWidget(pages, 1);
    contentLayout->addWidget(categories);

    auto* layout = new QVBoxLayout(d->dialog);
    layout->addLayout(contentLayout, 1);
    layout->addWidget(buttons);
}

SettingsDialog::~SettingsDialog() = default;

int SettingsDialog::exec() {
    return d->dialog ? d->dialog->exec() : QDialog::Rejected;
}

} // namespace ui
} // namespace remo
