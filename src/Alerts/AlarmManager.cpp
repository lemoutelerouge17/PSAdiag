#include "AlarmManager.h"

#include <QSettings>

AlarmManager::AlarmManager(QObject* parent)
    : QObject(parent)
{
    load();
}

QList<AlertRule> AlarmManager::defaultRules()
{
    QList<AlertRule> r;
    r << AlertRule{ QStringLiteral("rpm"),     tr("Régime moteur"),      tr("tr/min"), 0,    6500,  false, true, true };
    r << AlertRule{ QStringLiteral("coolant"), tr("Température moteur"), tr("°C"),     0,    110,   false, true, true };
    r << AlertRule{ QStringLiteral("speed"),   tr("Vitesse"),            tr("km/h"),   0,    130,   false, true, true };
    r << AlertRule{ QStringLiteral("voltage"), tr("Tension batterie"),   tr("V"),      11.8, 15.0,  true,  true, true };
    r << AlertRule{ QStringLiteral("intake"),  tr("Température admission"), tr("°C"),  0,    70,    false, true, false };
    return r;
}

void AlarmManager::load()
{
    QSettings cfg;
    m_rules = defaultRules();

    cfg.beginGroup(QStringLiteral("alarms"));
    m_popups = cfg.value(QStringLiteral("popups"), true).toBool();
    m_cooldownSec = cfg.value(QStringLiteral("cooldown"), 30).toInt();
    for (AlertRule& rule : m_rules) {
        cfg.beginGroup(rule.key);
        rule.enabled = cfg.value(QStringLiteral("enabled"), rule.enabled).toBool();
        rule.minValue = cfg.value(QStringLiteral("min"), rule.minValue).toDouble();
        rule.maxValue = cfg.value(QStringLiteral("max"), rule.maxValue).toDouble();
        cfg.endGroup();
    }
    cfg.endGroup();
}

void AlarmManager::save() const
{
    QSettings cfg;
    cfg.beginGroup(QStringLiteral("alarms"));
    cfg.setValue(QStringLiteral("popups"), m_popups);
    cfg.setValue(QStringLiteral("cooldown"), m_cooldownSec);
    for (const AlertRule& rule : m_rules) {
        cfg.beginGroup(rule.key);
        cfg.setValue(QStringLiteral("enabled"), rule.enabled);
        cfg.setValue(QStringLiteral("min"), rule.minValue);
        cfg.setValue(QStringLiteral("max"), rule.maxValue);
        cfg.endGroup();
    }
    cfg.endGroup();
}

void AlarmManager::setRules(const QList<AlertRule>& rules)
{
    m_rules = rules;
    save();
}

void AlarmManager::setPopupsEnabled(bool on)
{
    m_popups = on;
    save();
}

void AlarmManager::check(const VehicleSample& s)
{
    for (const AlertRule& rule : std::as_const(m_rules)) {
        if (!rule.enabled)
            continue;

        double value = VehicleSample::NaN;
        if (rule.key == QLatin1String("rpm"))          value = s.rpm;
        else if (rule.key == QLatin1String("coolant")) value = s.coolant;
        else if (rule.key == QLatin1String("speed"))   value = s.speed;
        else if (rule.key == QLatin1String("voltage")) value = s.batteryVoltage;
        else if (rule.key == QLatin1String("intake"))  value = s.intakeTemp;

        if (VehicleSample::valid(value))
            evaluate(rule, value);
    }
}

void AlarmManager::evaluate(const AlertRule& rule, double value)
{
    double threshold = 0;
    QString message;

    if (rule.maxEnabled && value > rule.maxValue) {
        threshold = rule.maxValue;
        message = tr("%1 trop élevé(e) : %2 %3 (seuil %4 %3)")
                      .arg(rule.name).arg(value, 0, 'f', 1).arg(rule.unit).arg(threshold);
    } else if (rule.minEnabled && value < rule.minValue) {
        threshold = rule.minValue;
        message = tr("%1 trop faible : %2 %3 (seuil %4 %3)")
                      .arg(rule.name).arg(value, 0, 'f', 1).arg(rule.unit).arg(threshold);
    } else {
        return;
    }

    // Anti-rafale : une alarme par type toutes les N secondes.
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime last = m_lastFired.value(rule.key);
    if (last.isValid() && last.secsTo(now) < m_cooldownSec)
        return;
    m_lastFired.insert(rule.key, now);

    emit alarmTriggered(rule.key, rule.name, message, threshold, value);
}
