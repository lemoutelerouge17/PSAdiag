#pragma once

#include "Models/VehicleSample.h"

#include <QDateTime>
#include <QHash>
#include <QObject>

struct AlertRule
{
    QString key;          // identifiant (rpm, coolant, ...)
    QString name;         // libellé affiché
    QString unit;
    double minValue = 0;  // alarme si valeur < minValue (si minEnabled)
    double maxValue = 0;  // alarme si valeur > maxValue (si maxEnabled)
    bool minEnabled = false;
    bool maxEnabled = true;
    bool enabled = true;
};

// Surveillance des seuils ; paramètres persistés via QSettings (groupe "alarms").
class AlarmManager : public QObject
{
    Q_OBJECT

public:
    explicit AlarmManager(QObject* parent = nullptr);

    QList<AlertRule> rules() const { return m_rules; }
    void setRules(const QList<AlertRule>& rules);

    void setCooldownSeconds(int s) { m_cooldownSec = qMax(1, s); }
    int cooldownSeconds() const { return m_cooldownSec; }

    bool popupsEnabled() const { return m_popups; }
    void setPopupsEnabled(bool on);

    void check(const VehicleSample& sample);

    void load();
    void save() const;
    static QList<AlertRule> defaultRules();

signals:
    void alarmTriggered(const QString& key, const QString& title, const QString& message,
                        double threshold, double measured);

private:
    void evaluate(const AlertRule& rule, double value);

    QList<AlertRule> m_rules;
    QHash<QString, QDateTime> m_lastFired;
    int m_cooldownSec = 30;
    bool m_popups = true;
};
