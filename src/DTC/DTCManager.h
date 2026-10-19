#pragma once

#include "DTC.h"

#include <QHash>
#include <QList>
#include <QString>

// Décodage des codes défauts OBD-II génériques (SAE J2012).
class DTCManager
{
public:
    // Description française d'un code générique, ou description de la famille.
    static QString describe(const QString& code);

    // Sévérité estimée (1..3) pour l'affichage.
    static int severity(const QString& code);

    // Conversion de deux octets bruts en code "P0420".
    static QString bytesToCode(quint8 a, quint8 b);

    // Analyse d'une réponse mode 03 / 07 / 0A (CAN ou non-CAN, multi-calculateurs).
    static QList<DTC> parseResponse(const QString& raw, DTC::Status status = DTC::Status::Stored);

private:
    static const QHash<QString, QString>& dictionary();
};
