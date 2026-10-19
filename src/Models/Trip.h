#pragma once

#include <QDateTime>

struct Trip
{
    int id = -1;
    int vehicleId = -1;
    QDateTime startTime;
    QDateTime endTime;
    double distance = 0;   // km
    double avgSpeed = 0;   // km/h (temps de roulage uniquement)
    double maxSpeed = 0;   // km/h
    double avgRpm = 0;     // tr/min
    double fuelUsed = 0;   // litres (estimation à partir du débit d'air)
};
