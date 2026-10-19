#include "TripsWidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

TripsWidget::TripsWidget(QWidget* parent)
    : QWidget(parent)
{
    m_startStop = new QPushButton(tr("Démarrer un trajet"));
    m_startStop->setMinimumWidth(160);
    auto* exportButton = new QPushButton(tr("Exporter CSV…"));
    m_delete = new QPushButton(tr("Supprimer"));

    m_live = new QLabel(tr("Aucun trajet en cours. Le trajet utilise la vitesse lue en temps réel "
                           "(l'acquisition doit être démarrée)."));
    m_live->setWordWrap(true);

    m_table = new QTableWidget(0, 8);
    m_table->setHorizontalHeaderLabels({ tr("N°"), tr("Début"), tr("Durée"), tr("Distance"),
                                         tr("Vitesse moy."), tr("Vitesse max"), tr("Régime moy."),
                                         tr("Carburant estimé") });
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto* bar = new QHBoxLayout;
    bar->addWidget(m_startStop);
    bar->addWidget(exportButton);
    bar->addWidget(m_delete);
    bar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(m_live);
    layout->addWidget(m_table, 1);

    connect(m_startStop, &QPushButton::clicked, this, [this]() {
        if (m_running)
            emit stopRequested();
        else
            emit startRequested();
    });
    connect(exportButton, &QPushButton::clicked, this, &TripsWidget::exportRequested);
    connect(m_delete, &QPushButton::clicked, this, [this]() {
        const int row = m_table->currentRow();
        if (row < 0 || row >= m_trips.size())
            return;
        if (QMessageBox::question(this, tr("Supprimer le trajet"),
                                  tr("Supprimer définitivement le trajet n°%1 ?").arg(m_trips.at(row).id))
            == QMessageBox::Yes)
            emit deleteRequested(m_trips.at(row).id);
    });
}

QString TripsWidget::formatDuration(qint64 s)
{
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600)
        .arg((s % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}

void TripsWidget::setTrips(const QList<Trip>& trips)
{
    m_trips = trips;
    m_table->setRowCount(0);

    auto item = [](const QString& text) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(Qt::AlignCenter);
        return it;
    };

    for (const Trip& t : trips) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, item(QString::number(t.id)));
        m_table->setItem(row, 1, item(t.startTime.toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
        m_table->setItem(row, 2, item(formatDuration(t.startTime.secsTo(t.endTime))));
        m_table->setItem(row, 3, item(QStringLiteral("%1 km").arg(t.distance, 0, 'f', 2)));
        m_table->setItem(row, 4, item(QStringLiteral("%1 km/h").arg(t.avgSpeed, 0, 'f', 1)));
        m_table->setItem(row, 5, item(QStringLiteral("%1 km/h").arg(t.maxSpeed, 0, 'f', 0)));
        m_table->setItem(row, 6, item(QStringLiteral("%1 tr/min").arg(t.avgRpm, 0, 'f', 0)));
        m_table->setItem(row, 7, item(QStringLiteral("%1 L").arg(t.fuelUsed, 0, 'f', 2)));
    }
    m_delete->setEnabled(!trips.isEmpty());
}

void TripsWidget::setRunning(bool running)
{
    m_running = running;
    m_startStop->setText(running ? tr("Arrêter le trajet") : tr("Démarrer un trajet"));
    if (!running)
        m_live->setText(tr("Aucun trajet en cours."));
}

void TripsWidget::setCanStart(bool canStart)
{
    m_startStop->setEnabled(m_running || canStart);
}

void TripsWidget::setLiveTrip(const Trip& t, qint64 durationSec)
{
    m_live->setText(tr("<b>Trajet en cours</b> – durée %1 · distance %2 km · vitesse moyenne %3 km/h · "
                       "vitesse max %4 km/h · carburant estimé %5 L")
                        .arg(formatDuration(durationSec))
                        .arg(t.distance, 0, 'f', 2)
                        .arg(t.avgSpeed, 0, 'f', 1)
                        .arg(t.maxSpeed, 0, 'f', 0)
                        .arg(t.fuelUsed, 0, 'f', 2));
}
