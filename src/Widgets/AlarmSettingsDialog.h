#pragma once

#include "Alerts/AlarmManager.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

class AlarmSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlarmSettingsDialog(AlarmManager* manager, QWidget* parent = nullptr);

    void accept() override;

private:
    struct Row
    {
        AlertRule rule;
        QCheckBox* enabled;
        QDoubleSpinBox* min;
        QDoubleSpinBox* max;
    };

    AlarmManager* m_manager;
    QList<Row> m_rows;
    QCheckBox* m_popups;
    QSpinBox* m_cooldown;
};
