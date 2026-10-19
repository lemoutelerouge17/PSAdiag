#pragma once

#include "DTC/DTC.h"
#include "Models/Trip.h"
#include "Models/Vehicle.h"
#include "Models/VehicleSample.h"

#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>

struct SessionInfo
{
    int id = -1;
    int vehicleId = -1;
    QString vehicleName;
    QDateTime start;
    QDateTime end;
    int sampleCount = 0;

    QString label() const
    {
        return QStringLiteral("#%1  %2  –  %3  (%4 mesures)")
            .arg(id)
            .arg(start.toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                 vehicleName.isEmpty() ? QStringLiteral("véhicule inconnu") : vehicleName)
            .arg(sampleCount);
    }
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    bool open(const QString& path = QString());
    void close();
    bool isOpen() const;
    QString path() const { return m_path; }
    QString lastError() const { return m_lastError; }

    // --- Véhicules ---------------------------------------------------------
    int ensureVehicle(const QString& vin);           // crée si absent, renvoie l'id
    Vehicle vehicle(int id) const;
    Vehicle vehicleByVin(const QString& vin) const;
    QList<Vehicle> vehicles() const;
    bool saveVehicle(Vehicle& vehicle);              // insertion ou mise à jour
    bool deleteVehicle(int id);

    // --- Sessions ----------------------------------------------------------
    int startSession(int vehicleId, const QString& adapter, const QString& protocol);
    void endSession(int sessionId);
    void setSessionVehicle(int sessionId, int vehicleId);
    QList<SessionInfo> sessions(int vehicleId = -1) const;

    // --- Mesures -----------------------------------------------------------
    bool addSample(int sessionId, const VehicleSample& sample);
    QList<VehicleSample> samples(int sessionId, int limit = -1) const;
    QList<VehicleSample> loadHistory(int limit = 1000) const;

    // --- Défauts, alarmes, trajets ---------------------------------------
    bool addDtc(int vehicleId, int sessionId, const DTC& dtc);
    QList<DTC> lastDtcs(int vehicleId) const;
    bool addAlarm(int vehicleId, const QString& type, double threshold,
                  double measured, const QString& description);
    bool saveTrip(Trip& trip);
    QList<Trip> trips(int vehicleId = -1) const;
    bool deleteTrip(int tripId);

    // --- Divers ------------------------------------------------------------
    bool logExport(const QString& type, const QString& filename);
    bool refreshVehicleStatistics(int vehicleId);

signals:
    void vehiclesChanged();
    void sessionsChanged();
    void tripsChanged();
    void dtcHistoryChanged();

private:
    static VehicleSample sampleFromQuery(const QSqlQuery& q);
    static Vehicle vehicleFromQuery(const QSqlQuery& q);
    bool fail(const QSqlQuery& q, const QString& context) const;

    QSqlDatabase m_db;
    QString m_path;
    mutable QString m_lastError;
};
