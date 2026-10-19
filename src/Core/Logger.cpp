#include "Logger.h"

#include "AppInfo.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QTextStream>

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger() = default;

QString Logger::currentLogFile() const
{
    return m_file.fileName();
}

void Logger::openFileIfNeeded()
{
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    if (m_file.isOpen() && m_fileDate == today)
        return;

    if (m_file.isOpen())
        m_file.close();

    m_fileDate = today;
    m_file.setFileName(AppInfo::logDir() + QStringLiteral("/psadiag_%1.log").arg(today));
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::log(Level level, const QString& message)
{
    static const char* tags[] = { "DBG", "INF", "WRN", "ERR", "TX ", "RX " };

    const QString line = QStringLiteral("%1 [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             QString::fromLatin1(tags[static_cast<int>(level)]),
             message);

    {
        QMutexLocker lock(&m_mutex);
        openFileIfNeeded();
        if (m_file.isOpen()) {
            QTextStream out(&m_file);
            out << line << '\n';
        }
    }

    emit messageLogged(static_cast<int>(level), line);
}
