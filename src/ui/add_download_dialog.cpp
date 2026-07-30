#include "ui/add_download_dialog.h"

#include <QDialog>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStandardPaths>
#include <QUrl>

namespace remo {
namespace ui {

class AddDownloadDialog::Impl {
public:
    QDialog* dialog = nullptr;
    QLineEdit* urlEdit = nullptr;
    QLineEdit* filenameEdit = nullptr;
    QLineEdit* savePathEdit = nullptr;
    QSpinBox* connectionsSpinBox = nullptr;
    QSpinBox* prioritySpinBox = nullptr;
    QComboBox* categoryCombo = nullptr;
    QComboBox* startModeCombo = nullptr;
    QCheckBox* scheduleCheckBox = nullptr;
    QPushButton* cancelButton = nullptr;
    QPushButton* okButton = nullptr;

    ~Impl() {
        delete dialog;
    }
};

AddDownloadDialog::AddDownloadDialog(QWidget* parent)
    : d(std::make_unique<Impl>())
{
    d->dialog = new QDialog(parent);
    d->dialog->setWindowTitle(QObject::tr("إضافة رابط تحميل جديد"));
    d->dialog->setLayoutDirection(Qt::RightToLeft);
    d->dialog->setMinimumWidth(520);

    d->urlEdit = new QLineEdit(d->dialog);
    d->urlEdit->setPlaceholderText(QObject::tr("https://example.com/file.zip"));
    d->filenameEdit = new QLineEdit(d->dialog);
    d->savePathEdit = new QLineEdit(
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), d->dialog);

    d->connectionsSpinBox = new QSpinBox(d->dialog);
    d->connectionsSpinBox->setRange(1, 32);
    d->connectionsSpinBox->setValue(4);

    d->prioritySpinBox = new QSpinBox(d->dialog);
    d->prioritySpinBox->setRange(-10, 10);

    d->categoryCombo = new QComboBox(d->dialog);
    d->categoryCombo->addItems({QObject::tr("فيديو"), QObject::tr("برامج"),
                                QObject::tr("مستندات"), QObject::tr("صوت"),
                                QObject::tr("أخرى")});

    d->startModeCombo = new QComboBox(d->dialog);
    d->startModeCombo->addItems({QObject::tr("بدء فوري"), QObject::tr("إضافة للطابور فقط")});

    d->scheduleCheckBox = new QCheckBox(QObject::tr("جدولة وقت البدء لاحقًا"), d->dialog);

    auto* pasteButton = new QPushButton(QObject::tr("لصق من الحافظة"), d->dialog);
    auto* browseButton = new QPushButton(QObject::tr("تصفح..."), d->dialog);

    auto* urlRow = new QHBoxLayout();
    urlRow->addWidget(d->urlEdit, 1);
    urlRow->addWidget(pasteButton);

    auto* pathRow = new QHBoxLayout();
    pathRow->addWidget(d->savePathEdit, 1);
    pathRow->addWidget(browseButton);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow(QObject::tr("الرابط:"), urlRow);
    form->addRow(QObject::tr("اسم الملف:"), d->filenameEdit);
    form->addRow(QObject::tr("مسار الحفظ:"), pathRow);
    form->addRow(QObject::tr("الفئة:"), d->categoryCombo);

    auto* advanced = new QGroupBox(QObject::tr("خيارات متقدمة"), d->dialog);
    advanced->setCheckable(true);
    advanced->setChecked(false);
    auto* advancedForm = new QFormLayout(advanced);
    advancedForm->addRow(QObject::tr("عدد الاتصالات:"), d->connectionsSpinBox);
    advancedForm->addRow(QObject::tr("الأولوية:"), d->prioritySpinBox);
    advancedForm->addRow(QObject::tr("طريقة البدء:"), d->startModeCombo);
    advancedForm->addRow(QString(), d->scheduleCheckBox);

    auto* hint = new QLabel(
        QObject::tr("سيتم فحص الرابط في الخلفية لاحقًا قبل بدء التحميل الفعلي."), d->dialog);
    hint->setObjectName("secondaryText");
    hint->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok,
                                         Qt::Horizontal, d->dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("بدء التحميل"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("إلغاء"));

    auto* layout = new QVBoxLayout(d->dialog);
    layout->addLayout(form);
    layout->addWidget(advanced);
    layout->addWidget(hint);
    layout->addWidget(buttons);

    QObject::connect(pasteButton, &QPushButton::clicked, [this]() {
        d->urlEdit->setText(QApplication::clipboard()->text().trimmed());
    });
    QObject::connect(browseButton, &QPushButton::clicked, [this]() {
        const QString path = QFileDialog::getExistingDirectory(d->dialog, QObject::tr("اختر مسار الحفظ"),
                                                               d->savePathEdit->text());
        if (!path.isEmpty()) {
            d->savePathEdit->setText(path);
        }
    });
    QObject::connect(d->urlEdit, &QLineEdit::textChanged, [this](const QString& value) {
        if (!d->filenameEdit->text().isEmpty()) {
            return;
        }
        const QString filename = QUrl(value).fileName();
        if (!filename.isEmpty()) {
            d->filenameEdit->setText(filename);
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, [this]() {
        if (d->urlEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(d->dialog, QObject::tr("رابط مطلوب"),
                                 QObject::tr("أدخل رابط التحميل أولًا."));
            return;
        }
        d->dialog->accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, d->dialog, &QDialog::reject);
}

AddDownloadDialog::~AddDownloadDialog() = default;

std::string AddDownloadDialog::url() const {
    return d->urlEdit ? d->urlEdit->text().trimmed().toStdString() : "";
}

std::string AddDownloadDialog::filename() const {
    return d->filenameEdit ? d->filenameEdit->text().trimmed().toStdString() : "";
}

std::string AddDownloadDialog::savePath() const {
    return d->savePathEdit ? d->savePathEdit->text().trimmed().toStdString() : "";
}

int AddDownloadDialog::maxConnections() const {
    return d->connectionsSpinBox ? d->connectionsSpinBox->value() : 4;
}

int AddDownloadDialog::priority() const {
    return d->prioritySpinBox ? d->prioritySpinBox->value() : 0;
}

std::string AddDownloadDialog::category() const {
    return d->categoryCombo ? d->categoryCombo->currentText().toStdString() : "";
}

bool AddDownloadDialog::exec() {
    return d->dialog && d->dialog->exec() == QDialog::Accepted;
}

} // namespace ui
} // namespace remo
