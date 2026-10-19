#include "DTCManager.h"

#include "OBD/ObdParser.h"

QString DTCManager::bytesToCode(quint8 a, quint8 b)
{
    static const char families[] = { 'P', 'C', 'B', 'U' };
    const quint16 value = quint16((a << 8) | b);

    return QStringLiteral("%1%2%3%4%5")
        .arg(QString(QChar::fromLatin1(families[(value >> 14) & 0x3])))
        .arg((value >> 12) & 0x3)
        .arg((value >> 8) & 0xF, 0, 16)
        .arg((value >> 4) & 0xF, 0, 16)
        .arg(value & 0xF, 0, 16)
        .toUpper();
}

QList<DTC> DTCManager::parseResponse(const QString& raw, DTC::Status status)
{
    QList<DTC> result;
    QStringList seen;

    for (const QByteArray& msg : ObdParser::messages(raw)) {
        if (msg.isEmpty())
            continue;

        const quint8 service = quint8(msg.at(0));
        if (service != 0x43 && service != 0x47 && service != 0x4A)
            continue;

        QByteArray payload = msg.mid(1);

        // En CAN, le premier octet est le nombre de défauts (charge utile impaire).
        if (payload.size() % 2 != 0)
            payload.remove(0, 1);

        for (int i = 0; i + 1 < payload.size(); i += 2) {
            const quint8 a = quint8(payload.at(i));
            const quint8 b = quint8(payload.at(i + 1));
            if (a == 0 && b == 0)
                continue;

            const QString code = bytesToCode(a, b);
            if (seen.contains(code))
                continue;
            seen << code;

            DTC dtc;
            dtc.code = code;
            dtc.description = describe(code);
            dtc.status = status;
            dtc.severity = severity(code);
            result << dtc;
        }
    }
    return result;
}

QString DTCManager::describe(const QString& code)
{
    const QString c = code.trimmed().toUpper();
    const auto it = dictionary().constFind(c);
    if (it != dictionary().constEnd())
        return it.value();

    if (c.size() != 5)
        return QStringLiteral("Code invalide");

    const bool manufacturer = (c.at(1) == QLatin1Char('1') || c.at(1) == QLatin1Char('3'));
    const QString owner = manufacturer ? QStringLiteral("constructeur") : QStringLiteral("générique");

    QString family;
    switch (c.at(0).toLatin1()) {
    case 'P': family = QStringLiteral("Groupe motopropulseur"); break;
    case 'C': family = QStringLiteral("Châssis"); break;
    case 'B': family = QStringLiteral("Carrosserie"); break;
    case 'U': family = QStringLiteral("Réseau / communication"); break;
    default:  family = QStringLiteral("Inconnu"); break;
    }

    if (c.at(0) == QLatin1Char('P') && !manufacturer) {
        switch (c.at(2).toLatin1()) {
        case '0': case '1': family += QStringLiteral(" – dosage air/carburant"); break;
        case '2': family += QStringLiteral(" – injection"); break;
        case '3': family += QStringLiteral(" – allumage / ratés"); break;
        case '4': family += QStringLiteral(" – contrôle des émissions"); break;
        case '5': family += QStringLiteral(" – ralenti / vitesse"); break;
        case '6': family += QStringLiteral(" – calculateur / sorties"); break;
        case '7': case '8': case '9': family += QStringLiteral(" – transmission"); break;
        default: break;
        }
    }

    return QStringLiteral("%1 (code %2 non répertorié)").arg(family, owner);
}

int DTCManager::severity(const QString& code)
{
    const QString c = code.toUpper();
    // Ratés d'allumage, surchauffe, pression d'huile : critique.
    if (c.startsWith(QLatin1String("P030")) || c == QLatin1String("P0217")
        || c == QLatin1String("P0218") || c == QLatin1String("P0524")
        || c == QLatin1String("P0520"))
        return 3;
    if (c.startsWith(QLatin1Char('U')) || c.startsWith(QLatin1String("P01"))
        || c.startsWith(QLatin1String("P02")) || c.startsWith(QLatin1String("P06")))
        return 2;
    return 1;
}
