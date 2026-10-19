#include "BackupManager.h"

#include "Core/AppInfo.h"
#include "Core/Logger.h"
#include "DatabaseManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

BackupManager::BackupManager(DatabaseManager* database, QObject* parent)
    : QObject(parent)
    , m_database(database)
{
}

QString BackupManager::backupDirectory() const
{
    return AppInfo::backupDir();
}

QStringList BackupManager::backups() const
{
    QDir dir(backupDirectory());
    QStringList files = dir.entryList({ QStringLiteral("psadiag_*.db") }, QDir::Files, QDir::Time);
    for (QString& f : files)
        f = dir.absoluteFilePath(f);
    return files;
}

QString BackupManager::createBackup(const QString& destination)
{
    m_lastError.clear();

    if (!m_database || !m_database->isOpen()) {
        m_lastError = tr("Base de données non ouverte");
        return QString();
    }

    QString target = destination;
    if (target.isEmpty()) {
        target = backupDirectory()
               + QStringLiteral("/psadiag_%1.db")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    }
    if (QFile::exists(target))
        QFile::remove(target);

    // VACUUM INTO produit une copie cohérente sans fermer la base.
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("psadiag")));
    QString escaped = target;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));

    if (!q.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped))) {
        m_lastError = q.lastError().text();
        Logger::error(tr("Sauvegarde impossible : %1").arg(m_lastError));
        return QString();
    }

    Logger::info(tr("Sauvegarde créée : %1").arg(target));
    if (destination.isEmpty())
        rotate();

    emit backupCreated(target);
    return target;
}

void BackupManager::rotate()
{
    QStringList files = backups();
    while (files.size() > m_maxBackups) {
        const QString oldest = files.takeLast();
        QFile::remove(oldest);
        Logger::info(tr("Ancienne sauvegarde supprimée : %1").arg(QFileInfo(oldest).fileName()));
    }
}

bool BackupManager::restoreBackup(const QString& backupFile)
{
    m_lastError.clear();

    if (!QFile::exists(backupFile)) {
        m_lastError = tr("Fichier introuvable : %1").arg(backupFile);
        return false;
    }

    const QString dbPath = m_database->path().isEmpty() ? AppInfo::databasePath()
                                                         : m_database->path();

    // Filet de sécurité : sauvegarde de l'état actuel avant écrasement.
    if (m_database->isOpen())
        createBackup();

    m_database->close();

    const QString previous = dbPath + QStringLiteral(".before_restore");
    QFile::remove(previous);
    QFile::rename(dbPath, previous);
    QFile::remove(dbPath + QStringLiteral("-wal"));
    QFile::remove(dbPath + QStringLiteral("-shm"));

    if (!QFile::copy(backupFile, dbPath)) {
        m_lastError = tr("Copie impossible vers %1").arg(dbPath);
        QFile::rename(previous, dbPath);
        m_database->open(dbPath);
        return false;
    }
    QFile::setPermissions(dbPath, QFile::ReadOwner | QFile::WriteOwner);

    if (!m_database->open(dbPath)) {
        m_lastError = m_database->lastError();
        // Retour à la base précédente.
        m_database->close();
        QFile::remove(dbPath);
        QFile::rename(previous, dbPath);
        m_database->open(dbPath);
        return false;
    }

    QFile::remove(previous);
    Logger::info(tr("Base restaurée depuis %1").arg(backupFile));
    emit databaseRestored();
    return true;
}
