#include "StatisticsWidget.h"

#include "Utils/StatisticsManager.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

StatisticsWidget::StatisticsWidget(QWidget* parent)
    : QWidget(parent)
{
    m_source = new QComboBox;
    m_source->addItem(tr("Session en cours"), int(Source::LiveSession));
    m_source->addItem(tr("Session sélectionnée dans l'historique"), int(Source::HistorySession));

    auto* refresh = new QPushButton(tr("Calculer"));
    m_label = new QLabel;

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels({ tr("Grandeur"), tr("Minimum"), tr("Maximum"),
                                         tr("Moyenne"), tr("Mesures") });
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("Source :")));
    bar->addWidget(m_source);
    bar->addWidget(refresh);
    bar->addSpacing(12);
    bar->addWidget(m_label, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(m_table, 1);

    auto request = [this]() { emit refreshRequested(source()); };
    connect(refresh, &QPushButton::clicked, this, request);
    connect(m_source, &QComboBox::currentIndexChanged, this, request);
}

StatisticsWidget::Source StatisticsWidget::source() const
{
    return Source(m_source->currentData().toInt());
}

void StatisticsWidget::setSamples(const QList<VehicleSample>& samples, const QString& label)
{
    m_label->setText(samples.isEmpty() ? tr("Aucune mesure") : label);

    const QList<SeriesStats> stats = StatisticsManager::computeAll(samples);
    m_table->setRowCount(0);

    auto cell = [](const QString& text, bool right = true) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(right ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter));
        return item;
    };
    auto fmt = [](double v, const QString& unit) {
        if (!VehicleSample::valid(v))
            return QStringLiteral("--");
        const int d = (unit == QLatin1String("V") || unit == QLatin1String("g/s")) ? 2 : 1;
        return QStringLiteral("%1 %2").arg(v, 0, 'f', d).arg(unit);
    };

    for (const SeriesStats& st : stats) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, cell(st.name, false));
        m_table->setItem(row, 1, cell(fmt(st.min, st.unit)));
        m_table->setItem(row, 2, cell(fmt(st.max, st.unit)));
        m_table->setItem(row, 3, cell(fmt(st.average, st.unit)));
        m_table->setItem(row, 4, cell(QString::number(st.count)));
    }
}
