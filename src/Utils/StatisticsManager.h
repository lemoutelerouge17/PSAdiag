#pragma once

#include "Models/VehicleSample.h"

#include <QList>
#include <QString>

#include <functional>

struct SeriesStats
{
    QString name;
    QString unit;
    int count = 0;
    double min = VehicleSample::NaN;
    double max = VehicleSample::NaN;
    double average = VehicleSample::NaN;
};

class StatisticsManager
{
public:
    // Les valeurs NaN (non mesurées) sont ignorées.
    static double min(const QList<double>& values);
    static double max(const QList<double>& values);
    static double average(const QList<double>& values);

    static SeriesStats compute(const QList<VehicleSample>& samples,
                               const QString& name, const QString& unit,
                               const std::function<double(const VehicleSample&)>& field);

    // Statistiques de toutes les grandeurs d'une liste d'échantillons.
    static QList<SeriesStats> computeAll(const QList<VehicleSample>& samples);
};
