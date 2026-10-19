#include "SimulatorTransport.h"

#include "Logger.h"

#include <QRandomGenerator>
#include <QTimer>
#include <QtMath>

namespace
{
const char* kSimVin = "VF3LCYHZPHS123456";
}

SimulatorTransport::SimulatorTransport(QObject* parent)
    : Transport(parent)
{
    m_storedDtcs = { QStringLiteral("0133"), QStringLiteral("0420") };
}

bool SimulatorTransport::open()
{
    m_open = true;
    m_echo = true;
    m_spaces = true;
    m_headers = false;
    m_clock.start();
    Logger::info(tr("Simulateur OBD-II démarré"));
    emit connected();
    return true;
}

void SimulatorTransport::close()
{
    if (!m_open)
        return;
    m_open = false;
    Logger::info(tr("Simulateur OBD-II arrêté"));
    emit disconnected();
}

bool SimulatorTransport::isConnected() const
{
    return m_open;
}

QString SimulatorTransport::description() const
{
    return tr("Simulateur");
}

void SimulatorTransport::sendCommand(const QString& command)
{
    if (!m_open)
        return;

    Logger::tx(command);

    const QString cmd = command.trimmed().toUpper().remove(' ');
    QString response = handle(cmd);
    if (m_echo)
        response.prepend(command + QLatin1Char('\r'));

    const int delay = cmd.startsWith(QLatin1String("ATZ"))
                          ? 400
                          : 20 + QRandomGenerator::global()->bounded(40);

    QTimer::singleShot(delay, this, [this, response]() {
        if (!m_open)
            return;
        Logger::rx(QString(response).replace('\r', QStringLiteral(" | ")));
        emit responseReceived(response.trimmed());
    });
}

QString SimulatorTransport::hex(const QList<int>& bytes) const
{
    QStringList parts;
    for (int b : bytes)
        parts << QStringLiteral("%1").arg(b & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
    return parts.join(m_spaces ? QStringLiteral(" ") : QString());
}

QString SimulatorTransport::handle(const QString& cmd)
{
    if (cmd.startsWith(QLatin1String("AT"))) {
        const QString at = cmd.mid(2);
        if (at == QLatin1String("Z") || at == QLatin1String("WS")) {
            m_echo = true; m_spaces = true; m_headers = false;
            return QStringLiteral("\rELM327 v1.4b");
        }
        if (at == QLatin1String("E0")) { m_echo = false; return QStringLiteral("OK"); }
        if (at == QLatin1String("E1")) { m_echo = true; return QStringLiteral("OK"); }
        if (at == QLatin1String("S0")) { m_spaces = false; return QStringLiteral("OK"); }
        if (at == QLatin1String("S1")) { m_spaces = true; return QStringLiteral("OK"); }
        if (at == QLatin1String("H0")) { m_headers = false; return QStringLiteral("OK"); }
        if (at == QLatin1String("H1")) { m_headers = true; return QStringLiteral("OK"); }
        if (at == QLatin1String("I"))   return QStringLiteral("ELM327 v1.4b (simulateur)");
        if (at == QLatin1String("DPN")) return QStringLiteral("A6");
        if (at == QLatin1String("DP"))  return QStringLiteral("AUTO, ISO 15765-4 (CAN 11/500)");
        if (at == QLatin1String("RV")) {
            const double t = m_clock.elapsed() / 1000.0;
            return QStringLiteral("%1V").arg(13.9 + 0.2 * qSin(t / 7.0), 0, 'f', 1);
        }
        return QStringLiteral("OK");
    }

    if (cmd.startsWith(QLatin1String("ST")))
        return QStringLiteral("?");

    bool ok = false;
    if (cmd.size() >= 2) {
        const int mode = cmd.left(2).toInt(&ok, 16);
        if (!ok)
            return QStringLiteral("?");

        switch (mode) {
        case 0x01: {
            const int pid = cmd.mid(2, 2).toInt(&ok, 16);
            return ok ? handleMode01(pid) : QStringLiteral("?");
        }
        case 0x03:
        case 0x07:
        case 0x0A:
            return dtcResponse(mode);
        case 0x04:
            m_storedDtcs.clear();
            return QStringLiteral("44");
        case 0x09:
            if (cmd.mid(2, 2) == QLatin1String("02"))
                return vinResponse();
            break;
        default:
            break;
        }
    }
    return QStringLiteral("NO DATA");
}

QString SimulatorTransport::handleMode01(int pid)
{
    const double t = m_clock.elapsed() / 1000.0;
    // Cycle de conduite simulé : accélérations / décélérations périodiques.
    const double phase = 0.5 + 0.5 * qSin(t / 6.0);
    const double speed = qMax(0.0, 10 + 110 * phase + 3 * qSin(t * 1.3));
    const double rpm = 800 + 2600 * phase + 150 * qSin(t * 2.1);
    const double coolant = qMin(91.0, 20 + t * 1.2) + qSin(t / 3.0);
    const double throttle = qBound(0.0, 12 + 60 * qCos(t / 6.0) * (phase > 0.4 ? 1 : 0.2), 100.0);
    const double load = qBound(0.0, 20 + 55 * phase, 100.0);

    const int r = qRound(rpm * 4);

    switch (pid) {
    case 0x00: {
        // PIDs supportés 01-20 : 01 04 05 0A 0B 0C 0D 0F 10 11 1C 20
        quint32 bitmap = 0;
        for (int p : { 0x01, 0x04, 0x05, 0x0A, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x11, 0x1C, 0x20 })
            bitmap |= (1u << (32 - p));
        return hex({ 0x41, 0x00, int(bitmap >> 24), int(bitmap >> 16), int(bitmap >> 8), int(bitmap) });
    }
    case 0x01: return hex({ 0x41, 0x01, m_storedDtcs.isEmpty() ? 0x00 : 0x80 | int(m_storedDtcs.size()), 0x07, 0x65, 0x00 });
    case 0x04: return hex({ 0x41, 0x04, qRound(load * 255 / 100) });
    case 0x05: return hex({ 0x41, 0x05, qRound(coolant + 40) });
    case 0x0A: return hex({ 0x41, 0x0A, qRound((350 + 20 * phase) / 3) });
    case 0x0B: return hex({ 0x41, 0x0B, qRound(35 + 60 * phase) });
    case 0x0C: return hex({ 0x41, 0x0C, r >> 8, r & 0xFF });
    case 0x0D: return hex({ 0x41, 0x0D, qRound(speed) });
    case 0x0F: return hex({ 0x41, 0x0F, qRound(22 + 4 * qSin(t / 20.0) + 40) });
    case 0x10: {
        const int maf = qRound((3 + 35 * phase) * 100);
        return hex({ 0x41, 0x10, maf >> 8, maf & 0xFF });
    }
    case 0x11: return hex({ 0x41, 0x11, qRound(throttle * 255 / 100) });
    case 0x1C: return hex({ 0x41, 0x1C, 0x06 });
    case 0x20: return hex({ 0x41, 0x20, 0x00, 0x00, 0x00, 0x00 });
    default:   return QStringLiteral("NO DATA");
    }
}

QString SimulatorTransport::vinResponse() const
{
    QList<int> payload = { 0x49, 0x02, 0x01 };
    for (const char c : QByteArray(kSimVin))
        payload << int(c);

    // Réponse ISO-TP multitrame telle qu'affichée par un ELM327 (ATH0).
    const QString sep = m_spaces ? QStringLiteral(" ") : QString();
    QStringList lines;
    lines << QStringLiteral("%1").arg(payload.size(), 3, 16, QLatin1Char('0')).toUpper();
    lines << QStringLiteral("0:") + sep + hex(payload.mid(0, 6));
    int index = 1;
    for (int i = 6; i < payload.size(); i += 7, ++index)
        lines << QStringLiteral("%1:").arg(index % 16, 0, 16).toUpper() + sep + hex(payload.mid(i, 7));
    return lines.join(QLatin1Char('\r'));
}

QString SimulatorTransport::dtcResponse(int mode) const
{
    const int answer = mode + 0x40;
    if (mode != 0x03 || m_storedDtcs.isEmpty())
        return hex({ answer, 0x00 });

    QList<int> bytes = { answer, int(m_storedDtcs.size()) };
    for (const QString& code : m_storedDtcs) {
        bytes << code.left(2).toInt(nullptr, 16);
        bytes << code.mid(2, 2).toInt(nullptr, 16);
    }
    return hex(bytes);
}
