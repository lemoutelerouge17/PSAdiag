#include "TripManager.h"

#include "Core/Logger.h"

namespace
{
constexpr double kStoichiometricAfr = 14.7;     // essence
constexpr double kFuelDensity = 745.0;          // g/L
constexpr double kMaxGapSeconds = 10.0;         // au-delà : interruption d'acquisition
}

TripManager::TripManager(QObject* parent)
    : QObject(parent)
{
}

qint64 TripManager::durationSeconds() const
{
    if (!m_trip.startTime.isValid())
        return 0;
    const QDateTime end = m_running ? QDateTime::currentDateTime() : m_trip.endTime;
    return m_trip.startTime.secsTo(end);
}

void TripManager::startTrip(int vehicleId)
{
    m_trip = Trip();
    m_trip.vehicleId = vehicleId;
    m_trip.startTime = QDateTime::currentDateTime();
    m_lastTimestamp = QDateTime();
    m_movingSeconds = 0;
    m_speedTimeSum = 0;
    m_rpmSum = 0;
    m_rpmCount = 0;
    m_fuelGrams = 0;
    m_running = true;

    Logger::info(tr("Trajet démarré"));
    emit tripStarted();
}

Trip TripManager::stopTrip()
{
    if (!m_running)
        return m_trip;

    m_running = false;
    m_trip.endTime = QDateTime::currentDateTime();
    m_trip.avgSpeed = m_movingSeconds > 0 ? m_speedTimeSum / m_movingSeconds : 0;
    m_trip.avgRpm = m_rpmCount > 0 ? m_rpmSum / m_rpmCount : 0;
    m_trip.fuelUsed = m_fuelGrams / kFuelDensity;

    Logger::info(tr("Trajet terminé : %1 km").arg(m_trip.distance, 0, 'f', 2));
    emit tripStopped(m_trip);
    return m_trip;
}

void TripManager::update(const VehicleSample& s)
{
    if (!m_running)
        return;

    const QDateTime ts = s.timestamp.isValid() ? s.timestamp : QDateTime::currentDateTime();

    double dt = 0;
    if (m_lastTimestamp.isValid())
        dt = m_lastTimestamp.msecsTo(ts) / 1000.0;
    m_lastTimestamp = ts;

    if (dt <= 0 || dt > kMaxGapSeconds)
        dt = 0;

    if (VehicleSample::valid(s.speed)) {
        m_trip.distance += s.speed * dt / 3600.0;
        m_trip.maxSpeed = qMax(m_trip.maxSpeed, s.speed);
        if (s.speed > 1.0) {
            m_movingSeconds += dt;
            m_speedTimeSum += s.speed * dt;
        }
    }

    if (VehicleSample::valid(s.rpm) && s.rpm > 0) {
        m_rpmSum += s.rpm;
        ++m_rpmCount;
    }

    if (VehicleSample::valid(s.maf))
        m_fuelGrams += (s.maf / kStoichiometricAfr) * dt;

    m_trip.avgSpeed = m_movingSeconds > 0 ? m_speedTimeSum / m_movingSeconds : 0;
    m_trip.avgRpm = m_rpmCount > 0 ? m_rpmSum / m_rpmCount : 0;
    m_trip.fuelUsed = m_fuelGrams / kFuelDensity;

    emit tripUpdated(m_trip);
}
