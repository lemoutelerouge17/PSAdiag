#include "VehicleDialog.h"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QRegularExpression>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
// Codes constructeur (WMI) courants – indicatif.
QString manufacturerFromWmi(const QString& vin)
{
    static const QHash<QString, QString> wmi = {
        { "VF3", "Peugeot" }, { "VF7", "Citroën" }, { "VR1", "DS Automobiles" },
        { "VR3", "Peugeot" }, { "VR7", "Citroën" }, { "VF1", "Renault" }, { "VF6", "Renault (RVI)" },
        { "UU1", "Dacia" }, { "WVW", "Volkswagen" }, { "WV1", "Volkswagen Utilitaires" },
        { "WV2", "Volkswagen Bus" }, { "WAU", "Audi" }, { "TRU", "Audi (Hongrie)" },
        { "VSS", "Seat" }, { "TMB", "Škoda" }, { "WBA", "BMW" }, { "WMW", "Mini" },
        { "WDD", "Mercedes-Benz" }, { "W0L", "Opel" }, { "W0V", "Opel" },
        { "ZFA", "Fiat" }, { "ZAR", "Alfa Romeo" }, { "VNK", "Toyota (France)" },
        { "SB1", "Toyota (UK)" }, { "JTD", "Toyota" }, { "KMH", "Hyundai" },
        { "KNA", "Kia" }, { "SJN", "Nissan (UK)" }, { "VSK", "Nissan (Espagne)" },
        { "WF0", "Ford (Allemagne)" }, { "YV1", "Volvo" }, { "TSM", "Suzuki" },
    };
    return wmi.value(vin.left(3).toUpper());
}
}

bool VehicleDialog::isValidVin(const QString& vin, QString* reason)
{
    static const QRegularExpression re(QStringLiteral("^[A-HJ-NPR-Z0-9]{17}$"));
    if (vin.size() != 17) {
        if (reason) *reason = tr("Le VIN doit comporter 17 caractères (%1 saisis).").arg(vin.size());
        return false;
    }
    if (!re.match(vin).hasMatch()) {
        if (reason) *reason = tr("Le VIN ne peut contenir que des chiffres et des lettres (sauf I, O et Q).");
        return false;
    }
    return true;
}

VehicleDialog::VehicleDialog(const Vehicle& vehicle, QWidget* parent)
    : QDialog(parent)
    , m_vehicle(vehicle)
{
    setWindowTitle(vehicle.isValid() ? tr("Modifier le véhicule") : tr("Nouveau véhicule"));
    setMinimumWidth(460);

    m_vin = new QLineEdit(vehicle.vin);
    m_vin->setMaxLength(17);
    m_vin->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-HJ-NPR-Za-hj-npr-z0-9]{0,17}")), m_vin));
    m_vin->setPlaceholderText(tr("17 caractères"));

    m_vinInfo = new QLabel;
    m_vinInfo->setStyleSheet(QStringLiteral("color: gray;"));

    m_brand = new QLineEdit(vehicle.brand);
    m_model = new QLineEdit(vehicle.model);
    m_engine = new QLineEdit(vehicle.engine);
    m_engine->setPlaceholderText(tr("ex. 1.6 BlueHDi 120"));
    m_registration = new QLineEdit(vehicle.registration);
    m_registration->setPlaceholderText(tr("AA-123-AA"));

    m_year = new QSpinBox;
    m_year->setRange(1990, QDate::currentDate().year() + 1);
    m_year->setSpecialValueText(tr("non renseignée"));
    m_year->setMinimum(1989);
    m_year->setValue(vehicle.year > 0 ? vehicle.year : 1989);

    m_fuel = new QComboBox;
    m_fuel->setEditable(true);
    m_fuel->addItems({ QString(), tr("Essence"), tr("Diesel"), tr("Hybride"), tr("GPL"), tr("E85") });
    m_fuel->setCurrentText(vehicle.fuelType);

    m_mileage = new QSpinBox;
    m_mileage->setRange(0, 2000000);
    m_mileage->setSuffix(tr(" km"));
    m_mileage->setGroupSeparatorShown(true);
    m_mileage->setValue(vehicle.mileage);

    m_comments = new QPlainTextEdit(vehicle.comments);
    m_comments->setMaximumHeight(90);

    auto* form = new QFormLayout;
    form->addRow(tr("VIN :"), m_vin);
    form->addRow(QString(), m_vinInfo);
    form->addRow(tr("Marque :"), m_brand);
    form->addRow(tr("Modèle :"), m_model);
    form->addRow(tr("Motorisation :"), m_engine);
    form->addRow(tr("Immatriculation :"), m_registration);
    form->addRow(tr("Année :"), m_year);
    form->addRow(tr("Carburant :"), m_fuel);
    form->addRow(tr("Kilométrage :"), m_mileage);
    form->addRow(tr("Commentaires :"), m_comments);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &VehicleDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &VehicleDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    auto updateInfo = [this]() {
        const QString vin = m_vin->text().toUpper();
        if (m_vin->text() != vin) {
            const int pos = m_vin->cursorPosition();
            m_vin->setText(vin);
            m_vin->setCursorPosition(pos);
        }
        const QString maker = manufacturerFromWmi(vin);
        m_vinInfo->setText(maker.isEmpty() ? QString() : tr("Constructeur probable : %1").arg(maker));
        if (!maker.isEmpty() && m_brand->text().isEmpty())
            m_brand->setPlaceholderText(maker);
    };
    connect(m_vin, &QLineEdit::textEdited, this, updateInfo);
    updateInfo();
}

Vehicle VehicleDialog::vehicle() const
{
    Vehicle v = m_vehicle;
    v.vin = m_vin->text().trimmed().toUpper();
    v.brand = m_brand->text().trimmed().isEmpty() ? m_brand->placeholderText() : m_brand->text().trimmed();
    v.model = m_model->text().trimmed();
    v.engine = m_engine->text().trimmed();
    v.registration = m_registration->text().trimmed().toUpper();
    v.year = m_year->value() == m_year->minimum() ? 0 : m_year->value();
    v.fuelType = m_fuel->currentText().trimmed();
    v.mileage = m_mileage->value();
    v.comments = m_comments->toPlainText().trimmed();
    return v;
}

void VehicleDialog::accept()
{
    QString reason;
    if (!isValidVin(m_vin->text().trimmed().toUpper(), &reason)) {
        QMessageBox::warning(this, tr("VIN invalide"), reason);
        m_vin->setFocus();
        return;
    }
    QDialog::accept();
}
