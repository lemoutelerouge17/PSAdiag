#include "ObdParser.h"

#include <QRegularExpression>

#include <cmath>
#include <limits>

namespace
{
const QStringList kErrorTokens = {
    QStringLiteral("NO DATA"),
    QStringLiteral("UNABLE TO CONNECT"),
    QStringLiteral("CAN ERROR"),
    QStringLiteral("BUS ERROR"),
    QStringLiteral("BUS BUSY"),
    QStringLiteral("BUFFER FULL"),
    QStringLiteral("DATA ERROR"),
    QStringLiteral("FB ERROR"),
    QStringLiteral("LV RESET"),
    QStringLiteral("STOPPED"),
    QStringLiteral("ERROR"),
};

QString compact(const QString& s)
{
    QString out = s.toUpper();
    out.remove(QLatin1Char(' '));
    return out;
}

bool isHex(const QString& s)
{
    static const QRegularExpression re(QStringLiteral("^[0-9A-F]+$"));
    return !s.isEmpty() && re.match(s).hasMatch();
}

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

QStringList ObdParser::cleanLines(const QString& raw, const QString& sentCommand)
{
    QString text = raw;
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    text.remove(QLatin1Char('>'));

    const QString echo = compact(sentCommand);
    QStringList lines;

    for (const QString& part : text.split(QLatin1Char('\r'), Qt::SkipEmptyParts)) {
        const QString line = part.trimmed();
        if (line.isEmpty())
            continue;

        const QString upper = line.toUpper();
        if (!echo.isEmpty() && compact(line) == echo)
            continue;
        if (upper.startsWith(QLatin1String("SEARCHING")))
            continue;
        if (upper.startsWith(QLatin1String("BUS INIT")))
            continue;

        lines << line;
    }
    return lines;
}

bool ObdParser::isError(const QString& raw)
{
    return !errorText(raw).isEmpty();
}

QString ObdParser::errorText(const QString& raw)
{
    const QStringList lines = cleanLines(raw);
    if (lines.isEmpty())
        return QStringLiteral("NO RESPONSE");

    for (const QString& line : lines) {
        const QString upper = line.toUpper();
        if (upper == QLatin1String("?"))
            return QStringLiteral("COMMANDE INCONNUE");
        for (const QString& token : kErrorTokens) {
            if (upper.contains(token))
                return token;
        }
    }
    return QString();
}

QByteArray ObdParser::hexToBytes(const QString& hex)
{
    QString clean;
    clean.reserve(hex.size());
    for (const QChar c : hex) {
        if (c.isDigit() || (c.toUpper() >= QLatin1Char('A') && c.toUpper() <= QLatin1Char('F')))
            clean.append(c.toUpper());
    }
    if (clean.size() % 2 != 0)
        clean.chop(1);

    return QByteArray::fromHex(clean.toLatin1());
}

QList<QByteArray> ObdParser::messages(const QString& raw, const QString& sentCommand)
{
    static const QRegularExpression frameRe(QStringLiteral("^([0-9A-F]):([0-9A-F]*)$"));
    static const QRegularExpression lengthRe(QStringLiteral("^[0-9A-F]{3}$"));

    QList<QByteArray> result;
    QByteArray multi;
    int expected = 0;
    bool inMulti = false;

    auto flushMulti = [&]() {
        if (inMulti && !multi.isEmpty())
            result << (expected > 0 ? multi.left(expected) : multi);
        multi.clear();
        expected = 0;
        inMulti = false;
    };

    for (const QString& line : cleanLines(raw, sentCommand)) {
        const QString s = compact(line);

        if (lengthRe.match(s).hasMatch()) {
            // En-tête de longueur ISO-TP (ex. "014" = 20 octets).
            flushMulti();
            inMulti = true;
            expected = s.toInt(nullptr, 16);
            continue;
        }

        const QRegularExpressionMatch m = frameRe.match(s);
        if (m.hasMatch()) {
            inMulti = true;
            multi.append(hexToBytes(m.captured(2)));
            if (expected > 0 && multi.size() >= expected)
                flushMulti();
            continue;
        }

        if (isHex(s)) {
            flushMulti();
            result << hexToBytes(s);
        }
    }
    flushMulti();
    return result;
}

std::optional<QByteArray> ObdParser::pidData(const QString& raw, int pid)
{
    for (const QByteArray& msg : messages(raw)) {
        if (msg.size() >= 3
            && quint8(msg.at(0)) == 0x41
            && quint8(msg.at(1)) == pid) {
            return msg.mid(2);
        }
    }
    return std::nullopt;
}

std::optional<double> ObdParser::decodePid(int pid, const QByteArray& d)
{
    auto A = [&]() { return double(quint8(d.at(0))); };
    auto B = [&]() { return double(quint8(d.at(1))); };

    if (d.isEmpty())
        return std::nullopt;

    switch (pid) {
    case 0x04: return A() * 100.0 / 255.0;               // Charge moteur %
    case 0x05: return A() - 40.0;                        // Température liquide °C
    case 0x0A: return A() * 3.0;                         // Pression carburant kPa
    case 0x0B: return A();                               // Pression collecteur kPa
    case 0x0C: if (d.size() < 2) return std::nullopt;
               return (A() * 256.0 + B()) / 4.0;         // Régime tr/min
    case 0x0D: return A();                               // Vitesse km/h
    case 0x0F: return A() - 40.0;                        // Température air admission °C
    case 0x10: if (d.size() < 2) return std::nullopt;
               return (A() * 256.0 + B()) / 100.0;       // Débit d'air g/s
    case 0x11: return A() * 100.0 / 255.0;               // Papillon %
    case 0x2F: return A() * 100.0 / 255.0;               // Niveau carburant %
    case 0x42: if (d.size() < 2) return std::nullopt;
               return (A() * 256.0 + B()) / 1000.0;      // Tension calculateur V
    default:   return std::nullopt;
    }
}

static double decodeOrNaN(const QString& raw, int pid)
{
    const auto data = ObdParser::pidData(raw, pid);
    if (!data)
        return kNaN;
    const auto value = ObdParser::decodePid(pid, *data);
    return value ? *value : kNaN;
}

double ObdParser::parseRPM(const QString& raw)      { return decodeOrNaN(raw, 0x0C); }
double ObdParser::parseCoolant(const QString& raw)  { return decodeOrNaN(raw, 0x05); }
double ObdParser::parseSpeed(const QString& raw)    { return decodeOrNaN(raw, 0x0D); }
double ObdParser::parseThrottle(const QString& raw) { return decodeOrNaN(raw, 0x11); }

QString ObdParser::parseVIN(const QString& raw)
{
    QByteArray bytes;

    // CAN : un seul message "49 02 01 <17 octets>".
    // K-Line/J1850 : plusieurs lignes "49 02 0n xx xx xx xx".
    for (const QByteArray& msg : messages(raw)) {
        if (msg.size() > 3 && quint8(msg.at(0)) == 0x49 && quint8(msg.at(1)) == 0x02)
            bytes.append(msg.mid(3));
    }

    QString vin;
    for (const char c : bytes) {
        const QChar ch = QChar::fromLatin1(c).toUpper();
        if ((ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) || ch.isDigit())
            vin.append(ch);
    }

    // Le VIN fait 17 caractères ; les octets de bourrage éventuels sont devant.
    if (vin.size() > 17)
        vin = vin.right(17);

    return vin.size() == 17 ? vin : QString();
}

std::optional<double> ObdParser::parseVoltage(const QString& raw)
{
    static const QRegularExpression re(QStringLiteral("(\\d+(?:\\.\\d+)?)\\s*V"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(raw);
    if (!m.hasMatch())
        return std::nullopt;

    bool ok = false;
    const double v = m.captured(1).toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

QSet<int> ObdParser::parseSupportedPids(const QString& raw, int basePid)
{
    QSet<int> pids;
    const auto data = pidData(raw, basePid);
    if (!data || data->size() < 4)
        return pids;

    const quint32 bitmap = (quint32(quint8(data->at(0))) << 24)
                         | (quint32(quint8(data->at(1))) << 16)
                         | (quint32(quint8(data->at(2))) << 8)
                         |  quint32(quint8(data->at(3)));

    for (int i = 0; i < 32; ++i) {
        if (bitmap & (1u << (31 - i)))
            pids.insert(basePid + i + 1);
    }
    return pids;
}

bool ObdParser::isCanProtocol(const QString& atdpnResponse)
{
    for (const QString& line : cleanLines(atdpnResponse)) {
        QString s = compact(line);
        if (s.startsWith(QLatin1Char('A')))
            s.remove(0, 1);
        if (s.isEmpty())
            continue;
        bool ok = false;
        const int proto = s.left(1).toInt(&ok, 16);
        if (ok)
            return proto >= 6 && proto <= 0xC;
    }
    return false;
}
