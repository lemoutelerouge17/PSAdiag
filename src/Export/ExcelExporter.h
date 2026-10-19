#pragma once

#include "DTC/DTC.h"
#include "Models/Trip.h"
#include "Models/VehicleSample.h"

#include <QList>
#include <QString>

// Export .xlsx natif (nécessite la compilation avec -DPSADIAG_WITH_XLSX=ON).
class ExcelExporter
{
public:
    static bool isAvailable();

    // Classeur avec feuilles "Mesures", "Défauts" et "Trajets".
    static bool exportWorkbook(const QString& filename,
                               const QList<VehicleSample>& samples,
                               const QList<DTC>& dtcs,
                               const QList<Trip>& trips,
                               QString* error = nullptr);
};
