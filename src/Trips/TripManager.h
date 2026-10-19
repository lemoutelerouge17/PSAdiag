#pragma once

#include "Models/Trip.h"
#include "Models/VehicleSample.h"

#include <QObject>

// Calcul en direct d'un trajet : distance (intégration vitesse × temps réel
// écoulé entre deux mesures), vitesses moyenne / maximale, consommation estimée.
class TripManager : public QObject
{
    Q_OBJECT

public:
    explicit TripManager(QObject* parent = nullptr);

    bool isRunning() const { return m_running; }

    void startTrip(int vehicleId);
    Trip stopTrip();
    void setVehicle(int vehicleId) { m_trip.vehicleId = vehicleId; }

    void update(const VehicleSample& sample);

    const Trip& current() const { return m_trip; }
    qint64 durationSeconds() const;

signals:
    void tripStarted();
    void tripUpdated(const Trip& trip);
    void tripStopped(const Trip& trip);

private:
    bool m_running = false;
    Trip m_trip;
    QDateTime m_lastTimestamp;
    double m_movingSeconds = 0;
    double m_speedTimeSum = 0;   // Σ v·dt (km/h·s)
    double m_rpmSum = 0;
    int m_rpmCount = 0;
    double m_fuelGrams = 0;
};
