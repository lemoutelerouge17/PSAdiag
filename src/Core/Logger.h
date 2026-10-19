#pragma once

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>

// Journal horodaté global : écrit dans un fichier quotidien et
// diffuse chaque ligne à l'interface (onglet Journal).
class Logger : public QObject
{
    Q_OBJECT

public:
    enum class Level { Debug, Info, Warning, Error, Tx, Rx };

    static Logger& instance();

    void log(Level level, const QString& message);

    static void debug(const QString& m)   { instance().log(Level::Debug, m); }
    static void info(const QString& m)    { instance().log(Level::Info, m); }
    static void warning(const QString& m) { instance().log(Level::Warning, m); }
    static void error(const QString& m)   { instance().log(Level::Error, m); }
    static void tx(const QString& m)      { instance().log(Level::Tx, m); }
    static void rx(const QString& m)      { instance().log(Level::Rx, m); }

    QString currentLogFile() const;

signals:
    void messageLogged(int level, const QString& line);

private:
    Logger();
    void openFileIfNeeded();

    QMutex m_mutex;
    QFile m_file;
    QString m_fileDate;
};
