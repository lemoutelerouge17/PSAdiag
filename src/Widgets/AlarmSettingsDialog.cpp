#include "AlarmSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

AlarmSettingsDialog::AlarmSettingsDialog(AlarmManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setWindowTitle(tr("Seuils d'alarme"));

    auto* box = new QGroupBox(tr("Seuils"));
    auto* grid = new QGridLayout(box);
    grid->addWidget(new QLabel(tr("<b>Grandeur</b>")), 0, 0);
    grid->addWidget(new QLabel(tr("<b>Minimum</b>")), 0, 1);
    grid->addWidget(new QLabel(tr("<b>Maximum</b>")), 0, 2);

    int line = 1;
    for (const AlertRule& rule : manager->rules()) {
        Row row;
        row.rule = rule;

        row.enabled = new QCheckBox(rule.name);
        row.enabled->setChecked(rule.enabled);

        row.min = new QDoubleSpinBox;
        row.min->setRange(-100, 20000);
        row.min->setDecimals(rule.unit == QLatin1String("V") ? 1 : 0);
        row.min->setSuffix(QLatin1Char(' ') + rule.unit);
        row.min->setValue(rule.minValue);
        row.min->setEnabled(rule.minEnabled);

        row.max = new QDoubleSpinBox;
        row.max->setRange(-100, 20000);
        row.max->setDecimals(rule.unit == QLatin1String("V") ? 1 : 0);
        row.max->setSuffix(QLatin1Char(' ') + rule.unit);
        row.max->setValue(rule.maxValue);
        row.max->setEnabled(rule.maxEnabled);

        grid->addWidget(row.enabled, line, 0);
        grid->addWidget(row.min, line, 1);
        grid->addWidget(row.max, line, 2);
        m_rows << row;
        ++line;
    }

    m_popups = new QCheckBox(tr("Afficher une fenêtre d'alerte"));
    m_popups->setChecked(manager->popupsEnabled());

    m_cooldown = new QSpinBox;
    m_cooldown->setRange(1, 3600);
    m_cooldown->setSuffix(tr(" s"));
    m_cooldown->setValue(manager->cooldownSeconds());

    auto* options = new QFormLayout;
    options->addRow(m_popups);
    options->addRow(tr("Délai minimal entre deux alertes identiques :"), m_cooldown);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::RestoreDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &AlarmSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AlarmSettingsDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this]() {
        const QList<AlertRule> defaults = AlarmManager::defaultRules();
        for (int i = 0; i < m_rows.size() && i < defaults.size(); ++i) {
            m_rows[i].enabled->setChecked(defaults[i].enabled);
            m_rows[i].min->setValue(defaults[i].minValue);
            m_rows[i].max->setValue(defaults[i].maxValue);
        }
    });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(box);
    layout->addLayout(options);
    layout->addWidget(buttons);
}

void AlarmSettingsDialog::accept()
{
    QList<AlertRule> rules;
    for (const Row& row : std::as_const(m_rows)) {
        AlertRule r = row.rule;
        r.enabled = row.enabled->isChecked();
        r.minValue = row.min->value();
        r.maxValue = row.max->value();
        rules << r;
    }
    m_manager->setCooldownSeconds(m_cooldown->value());
    m_manager->setPopupsEnabled(m_popups->isChecked());
    m_manager->setRules(rules);   // sauvegarde dans QSettings
    QDialog::accept();
}
