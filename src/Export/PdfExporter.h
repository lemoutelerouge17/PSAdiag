#pragma once

#include "DTC/DTC.h"
#include "Models/Vehicle.h"
#include "Models/VehicleSample.h"

#include <QImage>
#include <QList>
#include <QString>

struct ReportData
{
    QString title = QStringLiteral("Rapport de diagnostic OBD-II");
    Vehicle vehicle;
    QString adapter;
    QString protocol;
    QString sessionLabel;
    QList<VehicleSample> samples;
    QList<DTC> dtcs;
    QImage chart;              // capture du graphique (facultatif)
    int maxTableRows = 200;    // lignes de mesures détaillées
};

class PdfExporter
{
public:
    static bool exportReport(const QString& filename, const ReportData& data, QString* error = nullptr);
};
