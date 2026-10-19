#include "MigrationManager.h"

#include "Core/AppInfo.h"
#include "Core/Logger.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

bool MigrationManager::exec(QSqlDatabase& db, const QString& sql, QString* error)
{
    QSqlQuery q(db);
    if (q.exec(sql))
        return true;

    const QString msg = QStringLiteral("%1\n→ %2").arg(q.lastError().text(), sql.simplified());
    Logger::error(QStringLiteral("Migration SQL : ") + msg);
    if (error)
        *error = msg;
    return false;
}

int MigrationManager::version(QSqlDatabase& db)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1")))
        return 0;
    return q.next() ? q.value(0).toInt() : 0;
}

bool MigrationManager::setVersion(QSqlDatabase& db, int version, QString* error)
{
    return exec(db, QStringLiteral("DELETE FROM schema_version"), error)
        && exec(db, QStringLiteral("INSERT INTO schema_version(version) VALUES(%1)").arg(version), error);
}

bool MigrationManager::migrate(QSqlDatabase& db, QString* error)
{
    if (!exec(db, QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version(version INTEGER NOT NULL)"), error))
        return false;

    int current = version(db);
    if (current > CurrentVersion) {
        if (error)
            *error = QStringLiteral("La base (v%1) provient d'une version plus récente du logiciel (v%2).")
                         .arg(current).arg(CurrentVersion);
        return false;
    }

    using Step = bool (*)(QSqlDatabase&, QString*);
    const Step steps[] = { &MigrationManager::toV1, &MigrationManager::toV2, &MigrationManager::toV3 };

    while (current < CurrentVersion) {
        const int target = current + 1;
        Logger::info(QStringLiteral("Migration de la base v%1 → v%2").arg(current).arg(target));

        if (!db.transaction()) {
            if (error) *error = db.lastError().text();
            return false;
        }
        if (!steps[target - 1](db, error) || !setVersion(db, target, error)) {
            db.rollback();
            return false;
        }
        if (!db.commit()) {
            if (error) *error = db.lastError().text();
            db.rollback();
            return false;
        }
        current = target;
    }
    return true;
}

// v1 : véhicules, sessions, mesures, défauts, trajets.
bool MigrationManager::toV1(QSqlDatabase& db, QString* e)
{
    return exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS vehicles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vin TEXT NOT NULL UNIQUE,
            brand TEXT, model TEXT, engine TEXT, registration TEXT,
            year INTEGER, fuel_type TEXT, mileage INTEGER, comments TEXT,
            created_at TEXT, updated_at TEXT
        ))"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vehicle_id INTEGER REFERENCES vehicles(id) ON DELETE CASCADE,
            start_time TEXT, end_time TEXT,
            adapter TEXT, protocol TEXT,
            software_version TEXT, notes TEXT
        ))"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS samples (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
            timestamp TEXT NOT NULL,
            rpm REAL, coolant REAL, intake_temp REAL, speed REAL, throttle REAL,
            engine_load REAL, maf REAL, fuel_pressure REAL, battery_voltage REAL
        ))"), e)
        && exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_session ON samples(session_id)"), e)
        && exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_timestamp ON samples(timestamp)"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS dtc_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vehicle_id INTEGER REFERENCES vehicles(id) ON DELETE CASCADE,
            session_id INTEGER REFERENCES sessions(id) ON DELETE SET NULL,
            timestamp TEXT, dtc_code TEXT, description TEXT,
            status TEXT, severity INTEGER
        ))"), e)
        && exec(db, QStringLiteral("CREATE INDEX IF NOT EXISTS idx_dtc_vehicle ON dtc_history(vehicle_id)"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS trips (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vehicle_id INTEGER REFERENCES vehicles(id) ON DELETE CASCADE,
            start_time TEXT, end_time TEXT,
            distance REAL, avg_speed REAL, max_speed REAL, avg_rpm REAL
        ))"), e);
}

// v2 : alarmes, historique des exports, statistiques, consommation des trajets.
bool MigrationManager::toV2(QSqlDatabase& db, QString* e)
{
    return exec(db, QStringLiteral("ALTER TABLE trips ADD COLUMN fuel_used REAL"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS alarms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vehicle_id INTEGER REFERENCES vehicles(id) ON DELETE CASCADE,
            timestamp TEXT, alarm_type TEXT,
            threshold REAL, measured_value REAL, description TEXT
        ))"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS exports (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            export_date TEXT, export_type TEXT, filename TEXT
        ))"), e)
        && exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS vehicle_statistics (
            vehicle_id INTEGER PRIMARY KEY REFERENCES vehicles(id) ON DELETE CASCADE,
            total_distance REAL, total_engine_hours REAL, trip_count INTEGER,
            average_speed REAL, max_speed REAL, max_rpm REAL, last_sync TEXT
        ))"), e)
        && exec(db, QStringLiteral(R"(
        CREATE VIEW IF NOT EXISTS vw_vehicle_summary AS
        SELECT v.id, v.vin,
               COUNT(DISTINCT t.id) AS trip_count,
               COALESCE(SUM(t.distance), 0) AS total_distance,
               COALESCE(MAX(t.max_speed), 0) AS vmax
        FROM vehicles v
        LEFT JOIN trips t ON t.vehicle_id = v.id
        GROUP BY v.id)"), e);
}

// v3 : informations d'application (suivi des versions).
bool MigrationManager::toV3(QSqlDatabase& db, QString* e)
{
    return exec(db, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS application_info (
            database_version INTEGER,
            application_version TEXT,
            install_date TEXT
        ))"), e)
        && exec(db, QStringLiteral("INSERT INTO application_info VALUES(3, '%1', '%2')")
                        .arg(AppInfo::version(),
                             QDateTime::currentDateTime().toString(Qt::ISODate)), e);
}
