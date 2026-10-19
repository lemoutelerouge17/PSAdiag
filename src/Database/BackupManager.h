#pragma once

#include <QObject>
#include <QStringList>

class DatabaseManager;

// Sauvegardes de la base SQLite (cohérentes même base ouverte, via VACUUM INTO),
// rotation automatique et restauration.
class BackupManager : public QObject
{
    Q_OBJECT

public:
    explicit BackupManager(DatabaseManager* database, QObject* parent = nullptr);

    void setMaxBackups(int count) { m_maxBackups = qMax(1, count); }

    // Crée une sauvegarde horodatée ; renvoie le chemin (vide en cas d'échec).
    QString createBackup(const QString& destination = QString());
    bool restoreBackup(const QString& backupFile);

    QStringList backups() const;          // du plus récent au plus ancien
    QString backupDirectory() const;

    QString lastError() const { return m_lastError; }

signals:
    void backupCreated(const QString& path);
    void databaseRestored();

private:
    void rotate();

    DatabaseManager* m_database;
    int m_maxBackups = 30;
    QString m_lastError;
};
