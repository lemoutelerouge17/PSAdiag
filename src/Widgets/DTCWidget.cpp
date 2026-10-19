#include "DTCWidget.h"

#include "Models/DtcTableModel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

DTCWidget::DTCWidget(QWidget* parent)
    : QWidget(parent)
{
    m_model = new DtcTableModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);   // recherche dans toutes les colonnes

    m_table = new QTableView;
    m_table->setModel(m_proxy);
    m_table->setSortingEnabled(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(DtcTableModel::DescriptionColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(DtcTableModel::CodeColumn, QHeaderView::ResizeToContents);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Rechercher (ex. P0420, lambda…)"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    m_readButton = new QPushButton(tr("Lire les défauts"));
    m_clearButton = new QPushButton(tr("Effacer les défauts…"));
    m_exportButton = new QPushButton(tr("Exporter CSV…"));

    connect(m_readButton, &QPushButton::clicked, this, &DTCWidget::readRequested);
    connect(m_clearButton, &QPushButton::clicked, this, &DTCWidget::confirmClear);
    connect(m_exportButton, &QPushButton::clicked, this, &DTCWidget::exportRequested);

    m_summary = new QLabel(tr("Défauts non lus. Connectez l'adaptateur puis cliquez sur « Lire les défauts »."));
    m_summary->setWordWrap(true);

    auto* bar = new QHBoxLayout;
    bar->addWidget(m_readButton);
    bar->addWidget(m_clearButton);
    bar->addWidget(m_exportButton);
    bar->addStretch();
    bar->addWidget(m_search, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(m_summary);
    layout->addWidget(m_table, 1);

    setAdapterReady(false);
}

void DTCWidget::setDtcs(const QList<DTC>& dtcs, const QString& origin)
{
    m_model->setDtcs(dtcs);
    m_exportButton->setEnabled(!dtcs.isEmpty());

    int stored = 0, pending = 0;
    for (const DTC& d : dtcs)
        (d.status == DTC::Status::Pending ? pending : stored)++;

    QString text = dtcs.isEmpty()
        ? tr("Aucun code défaut.")
        : tr("%1 défaut(s) mémorisé(s), %2 en attente.").arg(stored).arg(pending);
    if (!origin.isEmpty())
        text += QStringLiteral("  ") + origin;
    m_summary->setText(text);
}

QList<DTC> DTCWidget::dtcs() const
{
    return m_model->dtcs();
}

void DTCWidget::setAdapterReady(bool ready)
{
    m_readButton->setEnabled(ready);
    m_clearButton->setEnabled(ready);
    m_exportButton->setEnabled(!m_model->dtcs().isEmpty());
}

void DTCWidget::confirmClear()
{
    const auto answer = QMessageBox::warning(
        this, tr("Effacer les codes défauts"),
        tr("Cette opération (mode 04) efface les codes défauts et les données figées, "
           "éteint le voyant moteur et réinitialise les moniteurs de préparation "
           "(contrôle technique).\n\nLe contact doit être mis, moteur à l'arrêt.\n\n"
           "Continuer ?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes)
        emit clearRequested();
}
