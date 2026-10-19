#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

struct DTC
{
    enum class Status { Stored, Pending, Permanent };

    QString code;          // ex. "P0420"
    QString description;
    Status status = Status::Stored;
    int severity = 1;      // 1 = info, 2 = attention, 3 = critique

    static QString statusText(Status s)
    {
        switch (s) {
        case Status::Stored:    return QStringLiteral("Mémorisé");
        case Status::Pending:   return QStringLiteral("En attente");
        case Status::Permanent: return QStringLiteral("Permanent");
        }
        return QString();
    }

    static QString statusKey(Status s)
    {
        switch (s) {
        case Status::Stored:    return QStringLiteral("stored");
        case Status::Pending:   return QStringLiteral("pending");
        case Status::Permanent: return QStringLiteral("permanent");
        }
        return QString();
    }
};

Q_DECLARE_METATYPE(DTC)
