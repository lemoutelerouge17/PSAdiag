#include "ObdEngine.h"

#include "Core/Logger.h"
#include "Core/SerialManager.h"
#include "Core/SimulatorTransport.h"
#include "DTC/DTCManager.h"
#include "ObdCommand.h"
#include "ObdParser.h"

#include <QRegularExpression>

namespace
{
constexpr int kMaxConsecutiveTimeouts = 4;

const QList<int> kDefaultPollPids = {
    Pid::RPM, Pid::Speed, Pid::Coolant, Pid::Throttle,
    Pid::EngineLoad, Pid::IntakeTemp, Pid::MAF, Pid::FuelPressure,
};
}

ObdEngine::ObdEngine(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<VehicleSample>();
    qRegisterMetaType<DTC>();
    qRegisterMetaType<QList<DTC>>();

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &ObdEngine::onTimeout);

    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, &ObdEngine::enqueuePollCycle);
}

ObdEngine::~ObdEngine()
{
    if (m_transport) {
        m_transport->disconnect(this);
        m_transport->close();
    }
}

// ---------------------------------------------------------------------------
// Connexion
// ---------------------------------------------------------------------------

bool ObdEngine::connectSerial(const QString& port, qint32 baudrate)
{
    auto serial = std::make_unique<SerialManager>();
    serial->setPort(port, baudrate);
    return attachTransport(std::move(serial));
}

bool ObdEngine::connectSimulator()
{
    return attachTransport(std::make_unique<SimulatorTransport>());
}

bool ObdEngine::attachTransport(std::unique_ptr<Transport> transport)
{
    disconnectAdapter();

    m_transport = std::move(transport);

    connect(m_transport.get(), &Transport::responseReceived, this, &ObdEngine::onResponse);
    connect(m_transport.get(), &Transport::errorOccurred, this, &ObdEngine::errorOccurred);
    connect(m_transport.get(), &Transport::disconnected, this, [this]() {
        // Adaptateur débranché pendant l'utilisation.
        if (m_state != State::Disconnected) {
            disconnectAdapter();
            emit errorOccurred(tr("Liaison avec l'adaptateur perdue"));
        }
    });

    if (!m_transport->open()) {
        m_transport->disconnect(this);
        m_transport.release()->deleteLater();
        return false;
    }

    initializeAdapter();
    return true;
}

void ObdEngine::disconnectAdapter()
{
    stopPolling();
    m_timeoutTimer.stop();
    m_queue.clear();
    m_busy = false;
    m_cycleQueued = false;
    m_consecutiveTimeouts = 0;

    if (m_transport) {
        m_transport->disconnect(this);
        m_transport->close();
        // deleteLater : on peut être appelé depuis un signal du transport.
        m_transport.release()->deleteLater();
    }

    m_identity.clear();
    m_protocol.clear();
    m_supported.clear();
    setState(State::Disconnected);
}

void ObdEngine::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void ObdEngine::initializeAdapter()
{
    setState(State::Initializing);
    Logger::info(tr("Initialisation de l'adaptateur (%1)").arg(m_transport->description()));

    enqueue(ObdCommand::Reset(),          Kind::Init, 0, 5000);
    enqueue(ObdCommand::EchoOff(),        Kind::Init);
    enqueue(ObdCommand::LineFeedOff(),    Kind::Init);
    enqueue(ObdCommand::SpacesOff(),      Kind::Init);
    enqueue(ObdCommand::HeadersOff(),     Kind::Init);
    enqueue(ObdCommand::AdaptiveTiming(), Kind::Init);
    enqueue(ObdCommand::AutoProtocol(),   Kind::Init);
    enqueue(ObdCommand::Identify(),       Kind::Identify);
    enqueue(ObdCommand::StnIdentify(),    Kind::StnIdentify);
    // La première requête OBD déclenche la recherche du protocole : délai long.
    enqueue(ObdCommand::SupportedPids(),  Kind::SupportedPids, 0x00, 15000);
    enqueue(ObdCommand::ProtocolNumber(), Kind::ProtocolNumber);
    enqueue(ObdCommand::ProtocolName(),   Kind::ProtocolName);
}

// ---------------------------------------------------------------------------
// File d'attente
// ---------------------------------------------------------------------------

void ObdEngine::enqueue(const QString& command, Kind kind, int pid, int timeoutMs)
{
    if (!m_transport)
        return;

    m_queue.enqueue(Request{ command, kind, pid, timeoutMs });
    if (!m_busy)
        sendNext();
}

void ObdEngine::sendNext()
{
    if (m_busy)
        return;   // une seule commande en vol à la fois

    if (!m_transport || m_queue.isEmpty()) {
        m_busy = false;
        return;
    }

    m_busy = true;
    m_current = m_queue.dequeue();
    m_transport->sendCommand(m_current.command);
    m_timeoutTimer.start(m_current.timeoutMs);
}

void ObdEngine::onResponse(const QString& data)
{
    if (!m_busy) {
        Logger::debug(tr("Réponse tardive ignorée : %1").arg(data.simplified()));
        return;
    }

    m_timeoutTimer.stop();
    m_consecutiveTimeouts = 0;

    const Request req = m_current;
    m_busy = false;

    emit rawResponse(req.command, data);
    handle(req, data);

    if (m_transport)
        sendNext();
}

void ObdEngine::onTimeout()
{
    if (!m_busy)
        return;

    const Request req = m_current;
    m_busy = false;

    Logger::warning(tr("Délai dépassé pour %1").arg(req.command));

    if (++m_consecutiveTimeouts >= kMaxConsecutiveTimeouts
        || (req.command == ObdCommand::Reset())) {
        disconnectAdapter();
        emit errorOccurred(tr("L'adaptateur ne répond plus (%1). Vérifiez le port, "
                              "le débit et l'alimentation.").arg(req.command));
        return;
    }

    handle(req, QString());
    if (m_transport)
        sendNext();
}

// ---------------------------------------------------------------------------
// Traitement des réponses
// ---------------------------------------------------------------------------

void ObdEngine::handle(const Request& req, const QString& data)
{
    switch (req.kind) {
    case Kind::Init:
    case Kind::Raw:
        break;

    case Kind::Identify:
        m_identity = ObdParser::cleanLines(data, req.command).join(QLatin1Char(' '));
        break;

    case Kind::StnIdentify:
        // OBDLink (STN11xx) répond par ex. "OBDLink SX r4.2" ; un ELM327 répond "?".
        if (!ObdParser::isError(data)) {
            const QString stn = ObdParser::cleanLines(data, req.command).join(QLatin1Char(' '));
            if (!stn.isEmpty())
                m_identity = stn;
        }
        break;

    case Kind::SupportedPids:
        if (ObdParser::isError(data)) {
            emit errorOccurred(tr("Aucun calculateur ne répond (%1). Le contact est-il mis ?")
                                   .arg(ObdParser::errorText(data)));
        } else {
            m_supported = ObdParser::parseSupportedPids(data, 0x00);
            Logger::info(tr("%1 PIDs supportés détectés").arg(m_supported.size()));
        }
        break;

    case Kind::ProtocolNumber:
        Logger::info(tr("Protocole %1 (%2)")
                         .arg(ObdParser::cleanLines(data, req.command).join(QLatin1Char(' ')),
                              ObdParser::isCanProtocol(data) ? QStringLiteral("CAN")
                                                             : QStringLiteral("non-CAN")));
        break;

    case Kind::ProtocolName: {
        m_protocol = ObdParser::cleanLines(data, req.command).join(QLatin1Char(' '));
        m_protocol.remove(QStringLiteral("AUTO, "));

        m_pollPids.clear();
        for (int pid : kDefaultPollPids) {
            if (m_supported.isEmpty() || m_supported.contains(pid))
                m_pollPids << pid;
        }

        setState(State::Ready);
        Logger::info(tr("Adaptateur prêt : %1 / %2").arg(m_identity, m_protocol));
        emit adapterReady(m_identity, m_protocol);
        break;
    }

    case Kind::PollPid: {
        const auto bytes = ObdParser::pidData(data, req.pid);
        const auto value = bytes ? ObdParser::decodePid(req.pid, *bytes) : std::nullopt;
        if (!value)
            break;

        switch (req.pid) {
        case Pid::RPM:          m_currentSample.rpm = *value; break;
        case Pid::Speed:        m_currentSample.speed = *value; break;
        case Pid::Coolant:      m_currentSample.coolant = *value; break;
        case Pid::Throttle:     m_currentSample.throttle = *value; break;
        case Pid::EngineLoad:   m_currentSample.engineLoad = *value; break;
        case Pid::IntakeTemp:   m_currentSample.intakeTemp = *value; break;
        case Pid::MAF:          m_currentSample.maf = *value; break;
        case Pid::FuelPressure: m_currentSample.fuelPressure = *value; break;
        default: break;
        }
        break;
    }

    case Kind::PollVoltage: {
        if (const auto v = ObdParser::parseVoltage(data))
            m_currentSample.batteryVoltage = *v;

        // Fin de cycle : on publie l'échantillon complet.
        m_currentSample.timestamp = QDateTime::currentDateTime();
        emit sampleReady(m_currentSample);
        m_currentSample = VehicleSample();
        m_cycleQueued = false;
        schedulePoll();
        break;
    }

    case Kind::Vin: {
        const QString vin = ObdParser::parseVIN(data);
        if (vin.isEmpty())
            emit errorOccurred(tr("VIN non disponible sur ce véhicule (mode 09 non supporté)"));
        else
            emit vinReceived(vin);
        break;
    }

    case Kind::StoredDtc:
        m_dtcBuffer = DTCManager::parseResponse(data, DTC::Status::Stored);
        break;

    case Kind::PendingDtc: {
        for (const DTC& d : DTCManager::parseResponse(data, DTC::Status::Pending)) {
            bool exists = false;
            for (const DTC& e : std::as_const(m_dtcBuffer))
                exists = exists || e.code == d.code;
            if (!exists)
                m_dtcBuffer << d;
        }
        emit dtcsReceived(m_dtcBuffer);
        break;
    }

    case Kind::ClearDtc: {
        bool ok = false;
        for (const QByteArray& msg : ObdParser::messages(data))
            ok = ok || (!msg.isEmpty() && quint8(msg.at(0)) == 0x44);
        emit dtcsCleared(ok);
        if (ok)
            requestDtcs();
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Acquisition cyclique
// ---------------------------------------------------------------------------

void ObdEngine::startPolling()
{
    if (!isReady()) {
        emit errorOccurred(tr("Adaptateur non prêt"));
        return;
    }
    if (m_polling)
        return;

    m_polling = true;
    emit pollingChanged(true);
    enqueuePollCycle();
}

void ObdEngine::stopPolling()
{
    m_pollTimer.stop();
    if (!m_polling)
        return;
    m_polling = false;
    emit pollingChanged(false);
}

void ObdEngine::schedulePoll()
{
    if (m_polling && isReady())
        m_pollTimer.start(m_pollIntervalMs);
}

void ObdEngine::enqueuePollCycle()
{
    if (!m_polling || !isReady() || m_cycleQueued)
        return;

    m_cycleQueued = true;
    m_currentSample = VehicleSample();

    for (int pid : std::as_const(m_pollPids))
        enqueue(ObdCommand::Mode01(pid), Kind::PollPid, pid, 3000);

    enqueue(ObdCommand::BatteryVoltage(), Kind::PollVoltage, 0, 2000);
}

// ---------------------------------------------------------------------------
// Requêtes à la demande
// ---------------------------------------------------------------------------

void ObdEngine::requestVin()
{
    if (!isReady()) {
        emit errorOccurred(tr("Adaptateur non prêt"));
        return;
    }
    enqueue(ObdCommand::ReadVIN(), Kind::Vin, 0, 5000);
}

void ObdEngine::requestDtcs()
{
    if (!isReady()) {
        emit errorOccurred(tr("Adaptateur non prêt"));
        return;
    }
    m_dtcBuffer.clear();
    enqueue(ObdCommand::ReadDTC(),    Kind::StoredDtc, 0, 5000);
    enqueue(ObdCommand::PendingDTC(), Kind::PendingDtc, 0, 5000);
}

void ObdEngine::clearDtcs()
{
    if (!isReady()) {
        emit errorOccurred(tr("Adaptateur non prêt"));
        return;
    }
    enqueue(ObdCommand::ClearDTC(), Kind::ClearDtc, 0, 8000);
}

void ObdEngine::sendRaw(const QString& command)
{
    if (!m_transport) {
        emit errorOccurred(tr("Aucun adaptateur connecté"));
        return;
    }
    // Outil OBD-II générique : seules les commandes de l'adaptateur (AT/ST) et les
    // services OBD-II standard 01 à 0A sont autorisés depuis le terminal.
    QString cmd = command.trimmed().toUpper();
    cmd.remove(QLatin1Char(' '));
    static const QRegularExpression allowed(QStringLiteral("^(AT|ST)[A-Z0-9]*$|^0[1-9A][0-9A-F]*$"));
    if (!allowed.match(cmd).hasMatch()) {
        emit errorOccurred(tr("Commande refusée : %1 (seuls AT…, ST… et les services OBD-II 01 à 0A "
                              "sont acceptés)").arg(cmd));
        return;
    }
    enqueue(cmd, Kind::Raw, 0, 5000);
}
