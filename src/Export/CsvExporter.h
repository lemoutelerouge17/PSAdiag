#pragma once

#include "DTC/DTC.h"
#include "Models/Trip.h"
#include "Models/VehicleSample.h"

#include <QList>
#include <QString>

// Export CSV compatible Excel (UTF-8 avec BOM, séparateur ';', virgule décimale).
class CsvExporter
{
public:
    static bool exportHistory(const QString& filename, const QList<VehicleSample>& samples,
                              QString* error = nullptr);
    static bool exportDtcs(const QString& filename, const QList<DTC>& dtcs, QString* error = nullptr);
    static bool exportTrips(const QString& filename, const QList<Trip>& trips, QString* error = nullptr);

    static QString number(double value, int decimals = 2);
};
