#include "DashboardWidget.h"

#include "GaugeWidget.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace
{
QString fmt(double v, int decimals, const QString& unit)
{
    return VehicleSample::valid(v) ? QStringLiteral("%1 %2").arg(v, 0, 'f', decimals).arg(unit)
                                   : QStringLiteral("--");
}
}

DashboardWidget::DashboardWidget(QWidget* parent)
    : QWidget(parent)
{
    m_rpmGauge = new GaugeWidget;
    m_rpmGauge->setTitle(tr("RÉGIME"));
    m_rpmGauge->setUnit(tr("tr/min"));
    m_rpmGauge->setRange(0, 8000);
    m_rpmGauge->setTickDivisor(1000);
    m_rpmGauge->setZones(5500, 6500);

    m_speedGauge = new GaugeWidget;
    m_speedGauge->setTitle(tr("VITESSE"));
    m_speedGauge->setUnit(tr("km/h"));
    m_speedGauge->setRange(0, 240);
    m_speedGauge->setMajorTicks(9);

    m_tempGauge = new GaugeWidget;
    m_tempGauge->setTitle(tr("TEMP. MOTEUR"));
    m_tempGauge->setUnit(tr("°C"));
    m_tempGauge->setRange(-20, 140);
    m_tempGauge->setZones(105, 115);

    m_throttleGauge = new GaugeWidget;
    m_throttleGauge->setTitle(tr("PAPILLON"));
    m_throttleGauge->setUnit(tr("%"));
    m_throttleGauge->setRange(0, 100);
    m_throttleGauge->setMajorTicks(11);

    auto* gauges = new QGridLayout;
    gauges->addWidget(m_rpmGauge, 0, 0);
    gauges->addWidget(m_speedGauge, 0, 1);
    gauges->addWidget(m_tempGauge, 1, 0);
    gauges->addWidget(m_throttleGauge, 1, 1);

    // --- Panneau d'informations ------------------------------------------
    m_voltage = makeValueLabel();
    m_load = makeValueLabel();
    m_intake = makeValueLabel();
    m_maf = makeValueLabel();
    m_fuelPressure = makeValueLabel();

    auto* measures = new QGroupBox(tr("Mesures complémentaires"));
    auto* mForm = new QFormLayout(measures);
    mForm->addRow(tr("Tension batterie :"), m_voltage);
    mForm->addRow(tr("Charge moteur :"), m_load);
    mForm->addRow(tr("Temp. admission :"), m_intake);
    mForm->addRow(tr("Débit d'air (MAF) :"), m_maf);
    mForm->addRow(tr("Pression carburant :"), m_fuelPressure);

    m_vin = makeValueLabel();
    m_vin->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_vehicle = makeValueLabel();
    m_dtcCount = makeValueLabel();
    m_connection = makeValueLabel();
    m_trip = new QLabel(tr("Aucun trajet en cours"));
    m_trip->setWordWrap(true);
    m_recording = new QLabel;

    auto* info = new QGroupBox(tr("Véhicule"));
    auto* iForm = new QFormLayout(info);
    iForm->addRow(tr("Adaptateur :"), m_connection);
    iForm->addRow(tr("VIN :"), m_vin);
    iForm->addRow(tr("Véhicule :"), m_vehicle);
    iForm->addRow(tr("Codes défauts :"), m_dtcCount);
    iForm->addRow(tr("Acquisition :"), m_recording);

    auto* tripBox = new QGroupBox(tr("Trajet"));
    auto* tLayout = new QVBoxLayout(tripBox);
    tLayout->addWidget(m_trip);

    auto* side = new QVBoxLayout;
    side->addWidget(info);
    side->addWidget(measures);
    side->addWidget(tripBox);
    side->addStretch();

    auto* sideWidget = new QWidget;
    sideWidget->setLayout(side);
    sideWidget->setMinimumWidth(300);
    sideWidget->setMaximumWidth(420);

    auto* main = new QHBoxLayout(this);
    main->addLayout(gauges, 3);
    main->addWidget(sideWidget, 1);

    setVin(QString());
    setVehicleName(QString());
    setDtcCount(-1);
    setConnectionState(tr("Déconnecté"), false);
    setRecording(false);
    clearValues();
}

QLabel* DashboardWidget::makeValueLabel()
{
    auto* l = new QLabel(QStringLiteral("--"));
    QFont f = l->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.1);
    l->setFont(f);
    return l;
}

void DashboardWidget::updateSample(const VehicleSample& s)
{
    m_rpmGauge->setValue(s.rpm);
    m_speedGauge->setValue(s.speed);
    m_tempGauge->setValue(s.coolant);
    m_throttleGauge->setValue(s.throttle);

    m_voltage->setText(fmt(s.batteryVoltage, 1, QStringLiteral("V")));
    m_load->setText(fmt(s.engineLoad, 0, QStringLiteral("%")));
    m_intake->setText(fmt(s.intakeTemp, 0, QStringLiteral("°C")));
    m_maf->setText(fmt(s.maf, 1, QStringLiteral("g/s")));
    m_fuelPressure->setText(fmt(s.fuelPressure, 0, QStringLiteral("kPa")));

    if (VehicleSample::valid(s.batteryVoltage)) {
        const bool bad = s.batteryVoltage < 11.8 || s.batteryVoltage > 15.0;
        m_voltage->setStyleSheet(bad ? QStringLiteral("color:#dc3232;") : QString());
    }
}

void DashboardWidget::clearValues()
{
    updateSample(VehicleSample());
}

void DashboardWidget::setVin(const QString& vin)
{
    m_vin->setText(vin.isEmpty() ? QStringLiteral("--") : vin);
}

void DashboardWidget::setVehicleName(const QString& name)
{
    m_vehicle->setText(name.isEmpty() ? QStringLiteral("--") : name);
}

void DashboardWidget::setDtcCount(int count)
{
    if (count < 0) {
        m_dtcCount->setText(tr("non lus"));
        m_dtcCount->setStyleSheet(QString());
    } else if (count == 0) {
        m_dtcCount->setText(tr("aucun"));
        m_dtcCount->setStyleSheet(QStringLiteral("color:#30a050;"));
    } else {
        m_dtcCount->setText(QString::number(count));
        m_dtcCount->setStyleSheet(QStringLiteral("color:#dc3232;"));
    }
}

void DashboardWidget::setConnectionState(const QString& text, bool ok)
{
    m_connection->setText(text);
    m_connection->setStyleSheet(ok ? QStringLiteral("color:#30a050;") : QString());
}

void DashboardWidget::setTripInfo(const QString& text)
{
    m_trip->setText(text);
}

void DashboardWidget::setRecording(bool recording)
{
    m_recording->setText(recording ? tr("● enregistrement") : tr("arrêtée"));
    m_recording->setStyleSheet(recording ? QStringLiteral("color:#dc3232; font-weight:bold;") : QString());
}
