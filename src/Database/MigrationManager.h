#pragma once

#include <QSqlDatabase>
#include <QString>

// Migrations successives du schéma SQLite (table schema_version).
class MigrationManager
{
public:
    static constexpr int CurrentVersion = 3;

    static int version(QSqlDatabase& db);
    static bool migrate(QSqlDatabase& db, QString* error = nullptr);

private:
    static bool exec(QSqlDatabase& db, const QString& sql, QString* error);
    static bool setVersion(QSqlDatabase& db, int version, QString* error);

    static bool toV1(QSqlDatabase& db, QString* error);
    static bool toV2(QSqlDatabase& db, QString* error);
    static bool toV3(QSqlDatabase& db, QString* error);
};
