#include "HistoryWidget.h"

#include "Database/DatabaseManager.h"
#include "Export/ExcelExporter.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Modèle tabulaire des mesures
// ---------------------------------------------------------------------------
class SampleTableModel : public QAbstractTableModel
{
public:
    using QAbstractTableModel::QAbstractTableModel;

    void setSamples(const QList<VehicleSample>& s)
    {
        beginResetModel();
        m_samples = s;
        endResetModel();
    }
    const QList<VehicleSample>& samples() const { return m_samples; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : int(m_samples.size());
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 10;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= m_samples.size())
            return {};

        const VehicleSample& s = m_samples.at(index.row());

        if (role == Qt::TextAlignmentRole)
            return int(index.column() == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter);

        auto num = [role](double v, int d) -> QVariant {
            if (!VehicleSample::valid(v))
                return role == Qt::DisplayRole ? QVariant(QStringLiteral("--")) : QVariant();
            return role == Qt::DisplayRole ? QVariant(QString::number(v, 'f', d)) : QVariant(v);
        };

        if (role != Qt::DisplayRole && role != Qt::UserRole)
            return {};

        switch (index.column()) {
        case 0: return role == Qt::DisplayRole
                           ? QVariant(s.timestamp.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")))
                           : QVariant(s.timestamp);
        case 1: return num(s.rpm, 0);
        case 2: return num(s.speed, 0);
        case 3: return num(s.coolant, 0);
        case 4: return num(s.intakeTemp, 0);
        case 5: return num(s.throttle, 1);
        case 6: return num(s.engineLoad, 1);
        case 7: return num(s.maf, 2);
        case 8: return num(s.fuelPressure, 0);
        case 9: return num(s.batteryVoltage, 1);
        default: return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override
    {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};
        static const char* headers[] = {
            "Horodatage", "tr/min", "km/h", "Temp. moteur °C", "Temp. admission °C",
            "Papillon %", "Charge %", "MAF g/s", "Pression carb. kPa", "Tension V"
        };
        return section >= 0 && section < 10 ? QString::fromUtf8(headers[section]) : QVariant();
    }

private:
    QList<VehicleSample> m_samples;
};

// ---------------------------------------------------------------------------

HistoryWidget::HistoryWidget(DatabaseManager* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    m_sessionCombo = new QComboBox;
    m_sessionCombo->setMinimumWidth(380);

    auto* refresh = new QPushButton(tr("Actualiser"));
    auto* replay = new QPushButton(tr("Afficher dans le graphique"));
    auto* csv = new QPushButton(tr("CSV…"));
    auto* pdf = new QPushButton(tr("Rapport PDF…"));
    auto* xlsx = new QPushButton(tr("Excel…"));
    xlsx->setVisible(ExcelExporter::isAvailable());

    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Filtrer (ex. 14:32, 2500…)"));
    m_filter->setClearButtonEnabled(true);

    m_model = new SampleTableModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterKeyColumn(-1);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setSortRole(Qt::UserRole);

    m_table = new QTableView;
    m_table->setModel(m_proxy);
    m_table->setSortingEnabled(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_info = new QLabel;

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Session :")));
    top->addWidget(m_sessionCombo, 1);
    top->addWidget(refresh);
    top->addWidget(replay);
    top->addWidget(csv);
    top->addWidget(pdf);
    top->addWidget(xlsx);

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(m_filter, 1);
    filterRow->addWidget(m_info);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);

    connect(m_sessionCombo, &QComboBox::currentIndexChanged, this, &HistoryWidget::loadSession);
    connect(refresh, &QPushButton::clicked, this, &HistoryWidget::reloadSessions);
    connect(replay, &QPushButton::clicked, this, [this]() { emit replayRequested(currentSamples()); });
    connect(csv, &QPushButton::clicked, this, &HistoryWidget::exportCsvRequested);
    connect(pdf, &QPushButton::clicked, this, &HistoryWidget::exportPdfRequested);
    connect(xlsx, &QPushButton::clicked, this, &HistoryWidget::exportExcelRequested);
    connect(m_filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    reloadSessions();
}

void HistoryWidget::setVehicleFilter(int vehicleId)
{
    m_vehicleFilter = vehicleId;
    reloadSessions();
}

void HistoryWidget::reloadSessions()
{
    const int previous = currentSessionId();

    m_sessionCombo->blockSignals(true);
    m_sessionCombo->clear();
    if (m_db && m_db->isOpen()) {
        for (const SessionInfo& s : m_db->sessions(m_vehicleFilter))
            m_sessionCombo->addItem(s.label(), s.id);
    }
    const int idx = m_sessionCombo->findData(previous);
    m_sessionCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_sessionCombo->blockSignals(false);

    loadSession();
}

void HistoryWidget::loadSession()
{
    const int id = currentSessionId();
    const QList<VehicleSample> samples = (id > 0 && m_db) ? m_db->samples(id) : QList<VehicleSample>();
    m_model->setSamples(samples);
    m_info->setText(tr("%1 mesure(s)").arg(samples.size()));
}

int HistoryWidget::currentSessionId() const
{
    return m_sessionCombo->currentData().isValid() ? m_sessionCombo->currentData().toInt() : -1;
}

QString HistoryWidget::currentSessionLabel() const
{
    return m_sessionCombo->currentText();
}

QList<VehicleSample> HistoryWidget::currentSamples() const
{
    return m_model->samples();
}
