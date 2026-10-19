#pragma once

#include <QDateTime>
#include <QMetaType>

#include <cmath>
#include <limits>

// Une acquisition complète (un cycle de lecture des PIDs).
// Les valeurs non disponibles valent NaN (et NULL en base).
struct VehicleSample
{
    static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

    QDateTime timestamp;

    double rpm            = NaN;   // tr/min
    double coolant        = NaN;   // °C
    double intakeTemp     = NaN;   // °C
    double speed          = NaN;   // km/h
    double throttle       = NaN;   // %
    double engineLoad     = NaN;   // %
    double maf            = NaN;   // g/s
    double fuelPressure   = NaN;   // kPa
    double batteryVoltage = NaN;   // V

    static bool valid(double v) { return !std::isnan(v); }
};

Q_DECLARE_METATYPE(VehicleSample)
