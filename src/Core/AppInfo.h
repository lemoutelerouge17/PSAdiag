#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

#ifndef PSADIAG_VERSION
#define PSADIAG_VERSION "1.0.0"
#endif

namespace AppInfo
{
inline QString name()          { return QStringLiteral("PSADiag"); }
inline QString organization()  { return QStringLiteral("PSADiag"); }
inline QString version()       { return QStringLiteral(PSADIAG_VERSION); }

// Dossier des données utilisateur (base, sauvegardes, journaux).
// Windows : C:/Users/<nom>/AppData/Roaming/PSADiag/PSADiag
inline QString dataDir()
{
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    return path;
}

inline QString databasePath() { return dataDir() + QStringLiteral("/psadiag.db"); }

inline QString backupDir()
{
    const QString path = dataDir() + QStringLiteral("/Backups");
    QDir().mkpath(path);
    return path;
}

inline QString logDir()
{
    const QString path = dataDir() + QStringLiteral("/Logs");
    QDir().mkpath(path);
    return path;
}
}
