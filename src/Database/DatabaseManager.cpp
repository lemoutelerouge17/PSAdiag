#include "DatabaseManager.h"

#include "Core/AppInfo.h"
#include "Core/Logger.h"
#include "MigrationManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
const QString kConnection = QStringLiteral("psadiag");

QString iso(const QDateTime& dt)
{
    return dt.isValid() ? dt.toString(Qt::ISODateWithMs) : QString();
}

QDateTime fromIso(const QVariant& v)
{
    return QDateTime::fromString(v.toString(), Qt::ISODateWithMs);
}

// NaN → NULL SQL
QVariant real(double v)
{
    return VehicleSample::valid(v) ? QVariant(v) : QVariant(QMetaType::fromType<double>());
}

double realOrNaN(const QVariant& v)
{
    return v.isNull() ? VehicleSample::NaN : v.toDouble();
}

QVariant idOrNull(int id)
{
    return id > 0 ? QVariant(id) : QVariant(QMetaType::fromType<int>());
}
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::fail(const QSqlQuery& q, const QString& context) const
{
    m_lastError = QStringLiteral("%1 : %2").arg(context, q.lastError().text());
    Logger::error(QStringLiteral("SQL ") + m_lastError);
    return false;
}

bool DatabaseManager::open(const QString& path)
{
    close();

    m_path = path.isEmpty() ? AppInfo::databasePath() : path;

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnection);
    m_db.setDatabaseName(m_path);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        Logger::error(QStringLiteral("Ouverture base impossible : ") + m_lastError);
        return false;
    }

    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    q.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));

    QString error;
    if (!MigrationManager::migrate(m_db, &error)) {
        m_lastError = error;
        m_db.close();
        return false;
    }

    // Sessions restées ouvertes après un arrêt brutal.
    q.exec(QStringLiteral("UPDATE sessions SET end_time = start_time WHERE end_time IS NULL"));

    Logger::info(QStringLiteral("Base ouverte : %1 (schéma v%2)")
                     .arg(m_path).arg(MigrationManager::version(m_db)));
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isValid()) {
        if (m_db.isOpen())
            m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(kConnection);
    }
}

bool DatabaseManager::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

// ===========================================================================
// Véhicules
// ===========================================================================

Vehicle DatabaseManager::vehicleFromQuery(const QSqlQuery& q)
{
    Vehicle v;
    v.id = q.value(QStringLiteral("id")).toInt();
    v.vin = q.value(QStringLiteral("vin")).toString();
    v.brand = q.value(QStringLiteral("brand")).toString();
    v.model = q.value(QStringLiteral("model")).toString();
    v.engine = q.value(QStringLiteral("engine")).toString();
    v.registration = q.value(QStringLiteral("registration")).toString();
    v.year = q.value(QStringLiteral("year")).toInt();
    v.fuelType = q.value(QStringLiteral("fuel_type")).toString();
    v.mileage = q.value(QStringLiteral("mileage")).toInt();
    v.comments = q.value(QStringLiteral("comments")).toString();
    v.createdAt = fromIso(q.value(QStringLiteral("created_at")));
    v.updatedAt = fromIso(q.value(QStringLiteral("updated_at")));
    return v;
}

int DatabaseManager::ensureVehicle(const QString& vin)
{
    const Vehicle existing = vehicleByVin(vin);
    if (existing.isValid())
        return existing.id;

    Vehicle v;
    v.vin = vin;
    return saveVehicle(v) ? v.id : -1;
}

Vehicle DatabaseManager::vehicle(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM vehicles WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return vehicleFromQuery(q);
    return {};
}

Vehicle DatabaseManager::vehicleByVin(const QString& vin) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM vehicles WHERE vin = ?"));
    q.addBindValue(vin);
    if (q.exec() && q.next())
        return vehicleFromQuery(q);
    return {};
}

QList<Vehicle> DatabaseManager::vehicles() const
{
    QList<Vehicle> list;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM vehicles ORDER BY brand, model, vin"))) {
        fail(q, QStringLiteral("Lecture véhicules"));
        return list;
    }
    while (q.next())
        list << vehicleFromQuery(q);
    return list;
}

bool DatabaseManager::saveVehicle(Vehicle& v)
{
    const QDateTime now = QDateTime::currentDateTime();
    QSqlQuery q(m_db);

    if (v.isValid()) {
        q.prepare(QStringLiteral(
            "UPDATE vehicles SET vin=?, brand=?, model=?, engine=?, registration=?, year=?, "
            "fuel_type=?, mileage=?, comments=?, updated_at=? WHERE id=?"));
    } else {
        q.prepare(QStringLiteral(
            "INSERT INTO vehicles(vin, brand, model, engine, registration, year, fuel_type, "
            "mileage, comments, updated_at, created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
    }

    q.addBindValue(v.vin.trimmed().toUpper());
    q.addBindValue(v.brand);
    q.addBindValue(v.model);
    q.addBindValue(v.engine);
    q.addBindValue(v.registration);
    q.addBindValue(v.year);
    q.addBindValue(v.fuelType);
    q.addBindValue(v.mileage);
    q.addBindValue(v.comments);
    q.addBindValue(iso(now));
    q.addBindValue(v.isValid() ? QVariant(v.id) : QVariant(iso(now)));

    if (!q.exec())
        return fail(q, QStringLiteral("Enregistrement véhicule"));

    if (!v.isValid()) {
        v.id = q.lastInsertId().toInt();
        v.createdAt = now;
    }
    v.updatedAt = now;
    emit vehiclesChanged();
    return true;
}

bool DatabaseManager::deleteVehicle(int id)
{
    // Les clés étrangères ON DELETE CASCADE suppriment sessions, mesures, défauts, trajets.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM vehicles WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return fail(q, QStringLiteral("Suppression véhicule"));

    emit vehiclesChanged();
    emit sessionsChanged();
    emit tripsChanged();
    emit dtcHistoryChanged();
    return true;
}

// ===========================================================================
// Sessions
// ===========================================================================

int DatabaseManager::startSession(int vehicleId, const QString& adapter, const QString& protocol)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sessions(vehicle_id, start_time, adapter, protocol, software_version) "
        "VALUES(?,?,?,?,?)"));
    q.addBindValue(idOrNull(vehicleId));
    q.addBindValue(iso(QDateTime::currentDateTime()));
    q.addBindValue(adapter);
    q.addBindValue(protocol);
    q.addBindValue(AppInfo::version());

    if (!q.exec()) {
        fail(q, QStringLiteral("Création session"));
        return -1;
    }
    emit sessionsChanged();
    return q.lastInsertId().toInt();
}

void DatabaseManager::endSession(int sessionId)
{
    if (sessionId <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sessions SET end_time = ? WHERE id = ?"));
    q.addBindValue(iso(QDateTime::currentDateTime()));
    q.addBindValue(sessionId);
    if (!q.exec())
        fail(q, QStringLiteral("Clôture session"));
    emit sessionsChanged();
}

void DatabaseManager::setSessionVehicle(int sessionId, int vehicleId)
{
    if (sessionId <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sessions SET vehicle_id = ? WHERE id = ?"));
    q.addBindValue(idOrNull(vehicleId));
    q.addBindValue(sessionId);
    if (!q.exec())
        fail(q, QStringLiteral("Association session/véhicule"));
    emit sessionsChanged();
}

QList<SessionInfo> DatabaseManager::sessions(int vehicleId) const
{
    QList<SessionInfo> list;
    QSqlQuery q(m_db);

    QString sql = QStringLiteral(
        "SELECT s.id, s.vehicle_id, s.start_time, s.end_time, "
        "       v.brand, v.model, v.vin, "
        "       (SELECT COUNT(*) FROM samples m WHERE m.session_id = s.id) AS n "
        "FROM sessions s LEFT JOIN vehicles v ON v.id = s.vehicle_id ");
    if (vehicleId > 0)
        sql += QStringLiteral("WHERE s.vehicle_id = ? ");
    sql += QStringLiteral("ORDER BY s.id DESC");

    q.prepare(sql);
    if (vehicleId > 0)
        q.addBindValue(vehicleId);

    if (!q.exec()) {
        fail(q, QStringLiteral("Lecture sessions"));
        return list;
    }

    while (q.next()) {
        SessionInfo s;
        s.id = q.value(0).toInt();
        s.vehicleId = q.value(1).isNull() ? -1 : q.value(1).toInt();
        s.start = fromIso(q.value(2));
        s.end = fromIso(q.value(3));
        s.vehicleName = QStringLiteral("%1 %2").arg(q.value(4).toString(), q.value(5).toString()).trimmed();
        if (s.vehicleName.isEmpty())
            s.vehicleName = q.value(6).toString();
        s.sampleCount = q.value(7).toInt();
        list << s;
    }
    return list;
}

// ===========================================================================
// Mesures
// ===========================================================================

VehicleSample DatabaseManager::sampleFromQuery(const QSqlQuery& q)
{
    VehicleSample s;
    s.timestamp = fromIso(q.value(QStringLiteral("timestamp")));
    s.rpm = realOrNaN(q.value(QStringLiteral("rpm")));
    s.coolant = realOrNaN(q.value(QStringLiteral("coolant")));
    s.intakeTemp = realOrNaN(q.value(QStringLiteral("intake_temp")));
    s.speed = realOrNaN(q.value(QStringLiteral("speed")));
    s.throttle = realOrNaN(q.value(QStringLiteral("throttle")));
    s.engineLoad = realOrNaN(q.value(QStringLiteral("engine_load")));
    s.maf = realOrNaN(q.value(QStringLiteral("maf")));
    s.fuelPressure = realOrNaN(q.value(QStringLiteral("fuel_pressure")));
    s.batteryVoltage = realOrNaN(q.value(QStringLiteral("battery_voltage")));
    return s;
}

bool DatabaseManager::addSample(int sessionId, const VehicleSample& s)
{
    if (sessionId <= 0)
        return false;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO samples(session_id, timestamp, rpm, coolant, intake_temp, speed, throttle, "
        "engine_load, maf, fuel_pressure, battery_voltage) VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(sessionId);
    q.addBindValue(iso(s.timestamp.isValid() ? s.timestamp : QDateTime::currentDateTime()));
    q.addBindValue(real(s.rpm));
    q.addBindValue(real(s.coolant));
    q.addBindValue(real(s.intakeTemp));
    q.addBindValue(real(s.speed));
    q.addBindValue(real(s.throttle));
    q.addBindValue(real(s.engineLoad));
    q.addBindValue(real(s.maf));
    q.addBindValue(real(s.fuelPressure));
    q.addBindValue(real(s.batteryVoltage));

    return q.exec() || fail(q, QStringLiteral("Enregistrement mesure"));
}

QList<VehicleSample> DatabaseManager::samples(int sessionId, int limit) const
{
    QList<VehicleSample> list;
    QSqlQuery q(m_db);
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT * FROM samples WHERE session_id = ? ORDER BY id LIMIT ?"));
    q.addBindValue(sessionId);
    q.addBindValue(limit);   // LIMIT -1 = pas de limite en SQLite

    if (!q.exec()) {
        fail(q, QStringLiteral("Lecture mesures"));
        return list;
    }
    while (q.next())
        list << sampleFromQuery(q);
    return list;
}

QList<VehicleSample> DatabaseManager::loadHistory(int limit) const
{
    QList<VehicleSample> list;
    QSqlQuery q(m_db);
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT * FROM (SELECT * FROM samples ORDER BY id DESC LIMIT ?) ORDER BY id"));
    q.addBindValue(limit);

    if (!q.exec()) {
        fail(q, QStringLiteral("Lecture historique"));
        return list;
    }
    while (q.next())
        list << sampleFromQuery(q);
    return list;
}

// ===========================================================================
// Défauts, alarmes, trajets
// ===========================================================================

bool DatabaseManager::addDtc(int vehicleId, int sessionId, const DTC& dtc)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO dtc_history(vehicle_id, session_id, timestamp, dtc_code, description, status, severity) "
        "VALUES(?,?,?,?,?,?,?)"));
    q.addBindValue(idOrNull(vehicleId));
    q.addBindValue(idOrNull(sessionId));
    q.addBindValue(iso(QDateTime::currentDateTime()));
    q.addBindValue(dtc.code);
    q.addBindValue(dtc.description);
    q.addBindValue(DTC::statusKey(dtc.status));
    q.addBindValue(dtc.severity);

    if (!q.exec())
        return fail(q, QStringLiteral("Enregistrement défaut"));
    emit dtcHistoryChanged();
    return true;
}

QList<DTC> DatabaseManager::lastDtcs(int vehicleId) const
{
    QList<DTC> list;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT dtc_code, description, status, severity FROM dtc_history "
        "WHERE vehicle_id = ? AND timestamp = (SELECT MAX(timestamp) FROM dtc_history WHERE vehicle_id = ?)"));
    q.addBindValue(vehicleId);
    q.addBindValue(vehicleId);
    if (!q.exec())
        return list;

    while (q.next()) {
        DTC d;
        d.code = q.value(0).toString();
        d.description = q.value(1).toString();
        const QString st = q.value(2).toString();
        d.status = st == QLatin1String("pending")   ? DTC::Status::Pending
                 : st == QLatin1String("permanent") ? DTC::Status::Permanent
                                                    : DTC::Status::Stored;
        d.severity = q.value(3).toInt();
        list << d;
    }
    return list;
}

bool DatabaseManager::addAlarm(int vehicleId, const QString& type, double threshold,
                               double measured, const QString& description)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO alarms(vehicle_id, timestamp, alarm_type, threshold, measured_value, description) "
        "VALUES(?,?,?,?,?,?)"));
    q.addBindValue(idOrNull(vehicleId));
    q.addBindValue(iso(QDateTime::currentDateTime()));
    q.addBindValue(type);
    q.addBindValue(threshold);
    q.addBindValue(measured);
    q.addBindValue(description);
    return q.exec() || fail(q, QStringLiteral("Enregistrement alarme"));
}

bool DatabaseManager::saveTrip(Trip& t)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO trips(vehicle_id, start_time, end_time, distance, avg_speed, max_speed, avg_rpm, fuel_used) "
        "VALUES(?,?,?,?,?,?,?,?)"));
    q.addBindValue(idOrNull(t.vehicleId));
    q.addBindValue(iso(t.startTime));
    q.addBindValue(iso(t.endTime));
    q.addBindValue(t.distance);
    q.addBindValue(t.avgSpeed);
    q.addBindValue(t.maxSpeed);
    q.addBindValue(t.avgRpm);
    q.addBindValue(t.fuelUsed);

    if (!q.exec())
        return fail(q, QStringLiteral("Enregistrement trajet"));

    t.id = q.lastInsertId().toInt();
    if (t.vehicleId > 0)
        refreshVehicleStatistics(t.vehicleId);
    emit tripsChanged();
    return true;
}

QList<Trip> DatabaseManager::trips(int vehicleId) const
{
    QList<Trip> list;
    QSqlQuery q(m_db);
    QString sql = QStringLiteral("SELECT * FROM trips ");
    if (vehicleId > 0)
        sql += QStringLiteral("WHERE vehicle_id = ? ");
    sql += QStringLiteral("ORDER BY id DESC");
    q.prepare(sql);
    if (vehicleId > 0)
        q.addBindValue(vehicleId);

    if (!q.exec()) {
        fail(q, QStringLiteral("Lecture trajets"));
        return list;
    }
    while (q.next()) {
        Trip t;
        t.id = q.value(QStringLiteral("id")).toInt();
        t.vehicleId = q.value(QStringLiteral("vehicle_id")).isNull()
                          ? -1 : q.value(QStringLiteral("vehicle_id")).toInt();
        t.startTime = fromIso(q.value(QStringLiteral("start_time")));
        t.endTime = fromIso(q.value(QStringLiteral("end_time")));
        t.distance = q.value(QStringLiteral("distance")).toDouble();
        t.avgSpeed = q.value(QStringLiteral("avg_speed")).toDouble();
        t.maxSpeed = q.value(QStringLiteral("max_speed")).toDouble();
        t.avgRpm = q.value(QStringLiteral("avg_rpm")).toDouble();
        t.fuelUsed = q.value(QStringLiteral("fuel_used")).toDouble();
        list << t;
    }
    return list;
}

bool DatabaseManager::deleteTrip(int tripId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM trips WHERE id = ?"));
    q.addBindValue(tripId);
    if (!q.exec())
        return fail(q, QStringLiteral("Suppression trajet"));
    emit tripsChanged();
    return true;
}

// ===========================================================================
// Divers
// ===========================================================================

bool DatabaseManager::logExport(const QString& type, const QString& filename)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO exports(export_date, export_type, filename) VALUES(?,?,?)"));
    q.addBindValue(iso(QDateTime::currentDateTime()));
    q.addBindValue(type);
    q.addBindValue(filename);
    return q.exec() || fail(q, QStringLiteral("Journal export"));
}

bool DatabaseManager::refreshVehicleStatistics(int vehicleId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO vehicle_statistics
            (vehicle_id, total_distance, total_engine_hours, trip_count,
             average_speed, max_speed, max_rpm, last_sync)
        SELECT ?,
               COALESCE((SELECT SUM(distance) FROM trips WHERE vehicle_id = ?), 0),
               COALESCE((SELECT SUM((julianday(end_time) - julianday(start_time)) * 24)
                         FROM sessions WHERE vehicle_id = ? AND end_time IS NOT NULL), 0),
               (SELECT COUNT(*) FROM trips WHERE vehicle_id = ?),
               COALESCE((SELECT AVG(avg_speed) FROM trips WHERE vehicle_id = ?), 0),
               COALESCE((SELECT MAX(max_speed) FROM trips WHERE vehicle_id = ?), 0),
               COALESCE((SELECT MAX(m.rpm) FROM samples m JOIN sessions s ON s.id = m.session_id
                         WHERE s.vehicle_id = ?), 0),
               ?
    )"));
    for (int i = 0; i < 7; ++i)
        q.addBindValue(vehicleId);
    q.addBindValue(iso(QDateTime::currentDateTime()));

    return q.exec() || fail(q, QStringLiteral("Statistiques véhicule"));
}
