#include "ConnectionWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>

ConnectionWidget::ConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    m_portList = new QComboBox;
    m_portList->setMinimumWidth(260);
    m_portList->setToolTip(tr("Port série de l'adaptateur (OBDLink SX : « USB Serial Port »)"));

    m_baudList = new QComboBox;
    for (int b : { 9600, 38400, 57600, 115200, 230400, 500000 })
        m_baudList->addItem(QString::number(b), b);
    m_baudList->setToolTip(tr("Débit : 115200 pour l'OBDLink SX, 38400 pour la plupart des ELM327"));

    m_simulate = new QCheckBox(tr("Simulation"));
    m_simulate->setToolTip(tr("Utiliser le simulateur intégré (sans véhicule)"));

    m_refreshButton = new QPushButton(tr("Actualiser"));
    m_connectButton = new QPushButton(tr("Connecter"));
    m_connectButton->setMinimumWidth(110);

    m_statusLed = new QLabel;
    m_statusLed->setFixedSize(14, 14);
    m_statusText = new QLabel(tr("Déconnecté"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->addWidget(new QLabel(tr("Port :")));
    layout->addWidget(m_portList, 1);
    layout->addWidget(m_refreshButton);
    layout->addWidget(new QLabel(tr("Débit :")));
    layout->addWidget(m_baudList);
    layout->addWidget(m_simulate);
    layout->addWidget(m_connectButton);
    layout->addSpacing(12);
    layout->addWidget(m_statusLed);
    layout->addWidget(m_statusText, 2);

    QSettings cfg;
    const int baud = cfg.value(QStringLiteral("connection/baud"), 115200).toInt();
    m_baudList->setCurrentIndex(qMax(0, m_baudList->findData(baud)));
    m_simulate->setChecked(cfg.value(QStringLiteral("connection/simulate"), false).toBool());

    connect(m_refreshButton, &QPushButton::clicked, this, &ConnectionWidget::refreshPorts);
    connect(m_connectButton, &QPushButton::clicked, this, &ConnectionWidget::onConnectClicked);
    connect(m_simulate, &QCheckBox::toggled, this, [this](bool on) {
        m_portList->setEnabled(!on);
        m_baudList->setEnabled(!on);
    });

    m_portList->setEnabled(!m_simulate->isChecked());
    m_baudList->setEnabled(!m_simulate->isChecked());

    refreshPorts();
    setEngineState(ObdEngine::State::Disconnected);
}

void ConnectionWidget::refreshPorts()
{
    const QString previous = m_portList->currentData().toString().isEmpty()
                                 ? QSettings().value(QStringLiteral("connection/port")).toString()
                                 : m_portList->currentData().toString();

    m_portList->clear();

    int preferred = -1;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& port : ports) {
        QString label = port.portName();
        if (!port.description().isEmpty())
            label += QStringLiteral(" – ") + port.description();
        if (!port.manufacturer().isEmpty())
            label += QStringLiteral(" (%1)").arg(port.manufacturer());

        m_portList->addItem(label, port.portName());

        // OBDLink SX : puce FTDI (VID 0403) ; on la présélectionne.
        const QString desc = (port.description() + port.manufacturer()).toLower();
        if (preferred < 0 && (desc.contains(QLatin1String("obdlink"))
                              || desc.contains(QLatin1String("scantool"))
                              || (port.hasVendorIdentifier() && port.vendorIdentifier() == 0x0403)))
            preferred = m_portList->count() - 1;
    }

    if (m_portList->count() == 0)
        m_portList->addItem(tr("Aucun port détecté"), QString());

    const int prevIndex = m_portList->findData(previous);
    if (prevIndex >= 0)
        m_portList->setCurrentIndex(prevIndex);
    else if (preferred >= 0)
        m_portList->setCurrentIndex(preferred);
}

void ConnectionWidget::onConnectClicked()
{
    if (m_state != ObdEngine::State::Disconnected) {
        emit disconnectRequested();
        return;
    }

    const QString port = m_portList->currentData().toString();
    const bool simulate = m_simulate->isChecked();
    if (!simulate && port.isEmpty()) {
        m_statusText->setText(tr("Sélectionnez un port ou activez la simulation"));
        return;
    }

    saveSettings();
    emit connectRequested(port, m_baudList->currentData().toInt(), simulate);
}

void ConnectionWidget::saveSettings() const
{
    QSettings cfg;
    cfg.setValue(QStringLiteral("connection/port"), m_portList->currentData().toString());
    cfg.setValue(QStringLiteral("connection/baud"), m_baudList->currentData().toInt());
    cfg.setValue(QStringLiteral("connection/simulate"), m_simulate->isChecked());
}

void ConnectionWidget::setEngineState(ObdEngine::State state)
{
    m_state = state;

    QString color;
    switch (state) {
    case ObdEngine::State::Disconnected:
        color = QStringLiteral("#888888");
        m_statusText->setText(tr("Déconnecté"));
        m_connectButton->setText(tr("Connecter"));
        break;
    case ObdEngine::State::Initializing:
        color = QStringLiteral("#f0a020");
        m_statusText->setText(tr("Initialisation de l'adaptateur…"));
        m_connectButton->setText(tr("Annuler"));
        break;
    case ObdEngine::State::Ready:
        color = QStringLiteral("#30c050");
        m_connectButton->setText(tr("Déconnecter"));
        break;
    }

    m_statusLed->setStyleSheet(QStringLiteral("background:%1; border-radius:7px;").arg(color));

    const bool idle = state == ObdEngine::State::Disconnected;
    m_simulate->setEnabled(idle);
    m_refreshButton->setEnabled(idle);
    m_portList->setEnabled(idle && !m_simulate->isChecked());
    m_baudList->setEnabled(idle && !m_simulate->isChecked());
}

void ConnectionWidget::setAdapterInfo(const QString& identity, const QString& protocol)
{
    m_statusText->setText(tr("Connecté – %1 – %2").arg(identity, protocol));
}
