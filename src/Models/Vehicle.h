#pragma once

#include <QDateTime>
#include <QString>

struct Vehicle
{
    int id = -1;
    QString vin;
    QString brand;
    QString model;
    QString engine;
    QString registration;
    int year = 0;
    QString fuelType;
    int mileage = 0;
    QString comments;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isValid() const { return id > 0; }

    QString displayName() const
    {
        QString name = QStringLiteral("%1 %2").arg(brand, model).trimmed();
        if (name.isEmpty())
            name = vin;
        if (!registration.isEmpty())
            name += QStringLiteral(" (%1)").arg(registration);
        return name;
    }
};
