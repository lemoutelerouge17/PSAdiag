#include "ExcelExporter.h"

#ifdef PSADIAG_HAS_XLSX
#include "xlsxdocument.h"
#include "xlsxformat.h"
#endif

bool ExcelExporter::isAvailable()
{
#ifdef PSADIAG_HAS_XLSX
    return true;
#else
    return false;
#endif
}

bool ExcelExporter::exportWorkbook(const QString& filename,
                                   const QList<VehicleSample>& samples,
                                   const QList<DTC>& dtcs,
                                   const QList<Trip>& trips,
                                   QString* error)
{
#ifdef PSADIAG_HAS_XLSX
    QXlsx::Document xlsx;

    QXlsx::Format header;
    header.setFontBold(true);
    header.setPatternBackgroundColor(QColor(25, 60, 120));
    header.setFontColor(Qt::white);

    auto writeOrBlank = [&](int row, int col, double v) {
        if (VehicleSample::valid(v))
            xlsx.write(row, col, v);
    };

    // --- Mesures ---------------------------------------------------------
    xlsx.renameSheet(QStringLiteral("Sheet1"), QStringLiteral("Mesures"));
    const QStringList h1 = { "Horodatage", "Régime (tr/min)", "Vitesse (km/h)", "Temp. moteur (°C)",
                             "Temp. admission (°C)", "Papillon (%)", "Charge (%)", "MAF (g/s)",
                             "Pression carburant (kPa)", "Tension (V)" };
    for (int c = 0; c < h1.size(); ++c)
        xlsx.write(1, c + 1, h1.at(c), header);

    QXlsx::Format dateFmt;
    dateFmt.setNumberFormat(QStringLiteral("dd/mm/yyyy hh:mm:ss"));

    int row = 2;
    for (const VehicleSample& s : samples) {
        xlsx.write(row, 1, s.timestamp, dateFmt);
        writeOrBlank(row, 2, s.rpm);
        writeOrBlank(row, 3, s.speed);
        writeOrBlank(row, 4, s.coolant);
        writeOrBlank(row, 5, s.intakeTemp);
        writeOrBlank(row, 6, s.throttle);
        writeOrBlank(row, 7, s.engineLoad);
        writeOrBlank(row, 8, s.maf);
        writeOrBlank(row, 9, s.fuelPressure);
        writeOrBlank(row, 10, s.batteryVoltage);
        ++row;
    }
    xlsx.setColumnWidth(1, 1, 20);
    xlsx.setColumnWidth(2, 10, 14);

    // --- Défauts ---------------------------------------------------------
    xlsx.addSheet(QStringLiteral("Défauts"));
    xlsx.write(1, 1, QStringLiteral("Code"), header);
    xlsx.write(1, 2, QStringLiteral("Description"), header);
    xlsx.write(1, 3, QStringLiteral("État"), header);
    row = 2;
    for (const DTC& d : dtcs) {
        xlsx.write(row, 1, d.code);
        xlsx.write(row, 2, d.description);
        xlsx.write(row, 3, DTC::statusText(d.status));
        ++row;
    }
    xlsx.setColumnWidth(2, 2, 60);

    // --- Trajets ---------------------------------------------------------
    xlsx.addSheet(QStringLiteral("Trajets"));
    const QStringList h3 = { "Début", "Fin", "Distance (km)", "Vitesse moy. (km/h)",
                             "Vitesse max (km/h)", "Régime moyen", "Carburant estimé (L)" };
    for (int c = 0; c < h3.size(); ++c)
        xlsx.write(1, c + 1, h3.at(c), header);
    row = 2;
    for (const Trip& t : trips) {
        xlsx.write(row, 1, t.startTime, dateFmt);
        xlsx.write(row, 2, t.endTime, dateFmt);
        xlsx.write(row, 3, t.distance);
        xlsx.write(row, 4, t.avgSpeed);
        xlsx.write(row, 5, t.maxSpeed);
        xlsx.write(row, 6, t.avgRpm);
        xlsx.write(row, 7, t.fuelUsed);
        ++row;
    }
    xlsx.setColumnWidth(1, 7, 18);

    xlsx.selectSheet(QStringLiteral("Mesures"));

    if (!xlsx.saveAs(filename)) {
        if (error)
            *error = QStringLiteral("Écriture impossible : %1").arg(filename);
        return false;
    }
    return true;
#else
    Q_UNUSED(filename);
    Q_UNUSED(samples);
    Q_UNUSED(dtcs);
    Q_UNUSED(trips);
    if (error)
        *error = QStringLiteral("Export Excel non compilé (option CMake PSADIAG_WITH_XLSX=ON).");
    return false;
#endif
}
