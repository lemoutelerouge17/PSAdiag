#pragma once

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

// Analyse des réponses ELM327 / STN11xx.
// Gère : écho, espaces activés ou non, lignes "SEARCHING...",
// réponses multi-calculateurs et trames ISO-TP multitrames (CAN).
class ObdParser
{
public:
    // Lignes utiles (sans écho, sans messages d'état de l'adaptateur).
    static QStringList cleanLines(const QString& raw, const QString& sentCommand = QString());

    // Vrai si l'adaptateur a renvoyé une erreur (NO DATA, ?, CAN ERROR...).
    static bool isError(const QString& raw);
    static QString errorText(const QString& raw);

    // Une entrée par message reçu (multitrames réassemblées).
    static QList<QByteArray> messages(const QString& raw, const QString& sentCommand = QString());

    static QByteArray hexToBytes(const QString& hex);

    // Octets de données d'une réponse mode 01 (après 41 <pid>).
    static std::optional<QByteArray> pidData(const QString& raw, int pid);

    // Conversion SAE J1979 des PIDs connus.
    static std::optional<double> decodePid(int pid, const QByteArray& data);

    // Raccourcis (renvoient NaN si la réponse est invalide).
    static double parseRPM(const QString& raw);
    static double parseCoolant(const QString& raw);
    static double parseSpeed(const QString& raw);
    static double parseThrottle(const QString& raw);

    static QString parseVIN(const QString& raw);
    static std::optional<double> parseVoltage(const QString& raw);

    // PIDs supportés à partir d'une réponse 0100 / 0120 / 0140...
    static QSet<int> parseSupportedPids(const QString& raw, int basePid);

    // Vrai si le numéro de protocole (ATDPN) correspond à un bus CAN.
    static bool isCanProtocol(const QString& atdpnResponse);
};
