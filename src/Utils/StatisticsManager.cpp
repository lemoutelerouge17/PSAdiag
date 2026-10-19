#include "StatisticsManager.h"

#include <QCoreApplication>

#include <algorithm>

namespace
{
QList<double> validOnly(const QList<double>& values)
{
    QList<double> out;
    out.reserve(values.size());
    for (double v : values)
        if (VehicleSample::valid(v))
            out << v;
    return out;
}
}

double StatisticsManager::min(const QList<double>& values)
{
    const QList<double> v = validOnly(values);
    return v.isEmpty() ? VehicleSample::NaN : *std::min_element(v.begin(), v.end());
}

double StatisticsManager::max(const QList<double>& values)
{
    const QList<double> v = validOnly(values);
    return v.isEmpty() ? VehicleSample::NaN : *std::max_element(v.begin(), v.end());
}

double StatisticsManager::average(const QList<double>& values)
{
    const QList<double> v = validOnly(values);
    if (v.isEmpty())
        return VehicleSample::NaN;

    double sum = 0;
    for (double x : v)
        sum += x;
    return sum / double(v.size());
}

SeriesStats StatisticsManager::compute(const QList<VehicleSample>& samples,
                                       const QString& name, const QString& unit,
                                       const std::function<double(const VehicleSample&)>& field)
{
    QList<double> values;
    values.reserve(samples.size());
    for (const VehicleSample& s : samples)
        values << field(s);

    SeriesStats st;
    st.name = name;
    st.unit = unit;
    st.count = int(validOnly(values).size());
    st.min = min(values);
    st.max = max(values);
    st.average = average(values);
    return st;
}

QList<SeriesStats> StatisticsManager::computeAll(const QList<VehicleSample>& s)
{
    auto t = [](const char* text) { return QCoreApplication::translate("Statistics", text); };

    return {
        compute(s, t("Régime moteur"),          QStringLiteral("tr/min"), [](const VehicleSample& x) { return x.rpm; }),
        compute(s, t("Vitesse"),                QStringLiteral("km/h"),   [](const VehicleSample& x) { return x.speed; }),
        compute(s, t("Température moteur"),     QStringLiteral("°C"),     [](const VehicleSample& x) { return x.coolant; }),
        compute(s, t("Température admission"),  QStringLiteral("°C"),     [](const VehicleSample& x) { return x.intakeTemp; }),
        compute(s, t("Papillon"),               QStringLiteral("%"),      [](const VehicleSample& x) { return x.throttle; }),
        compute(s, t("Charge moteur"),          QStringLiteral("%"),      [](const VehicleSample& x) { return x.engineLoad; }),
        compute(s, t("Débit d'air (MAF)"),      QStringLiteral("g/s"),    [](const VehicleSample& x) { return x.maf; }),
        compute(s, t("Pression carburant"),     QStringLiteral("kPa"),    [](const VehicleSample& x) { return x.fuelPressure; }),
        compute(s, t("Tension batterie"),       QStringLiteral("V"),      [](const VehicleSample& x) { return x.batteryVoltage; }),
    };
}
