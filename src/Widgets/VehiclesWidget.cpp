#include "VehiclesWidget.h"

#include "Database/DatabaseManager.h"
#include "VehicleDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

VehiclesWidget::VehiclesWidget(DatabaseManager* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    auto* add = new QPushButton(tr("Ajouter…"));
    auto* edit = new QPushButton(tr("Modifier…"));
    auto* remove = new QPushButton(tr("Supprimer"));
    auto* activate = new QPushButton(tr("Utiliser ce véhicule"));
    activate->setToolTip(tr("Associe la session en cours à ce véhicule (si le VIN n'a pas pu être lu)"));

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({ tr("VIN"), tr("Marque"), tr("Modèle"), tr("Motorisation"),
                                         tr("Immatriculation"), tr("Année"), tr("Kilométrage") });
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_details = new QLabel;
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::RichText);
    m_details->setMinimumHeight(90);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* bar = new QHBoxLayout;
    bar->addWidget(add);
    bar->addWidget(edit);
    bar->addWidget(remove);
    bar->addStretch();
    bar->addWidget(activate);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(m_table, 3);
    layout->addWidget(m_details, 1);

    connect(add, &QPushButton::clicked, this, &VehiclesWidget::addVehicle);
    connect(edit, &QPushButton::clicked, this, &VehiclesWidget::editVehicle);
    connect(remove, &QPushButton::clicked, this, &VehiclesWidget::deleteVehicle);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &VehiclesWidget::editVehicle);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &VehiclesWidget::updateDetails);
    connect(activate, &QPushButton::clicked, this, [this]() {
        if (const int id = selectedId(); id > 0)
            emit activateRequested(id);
    });

    reload();
}

int VehiclesWidget::selectedId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0))
        return -1;
    return m_table->item(row, 0)->data(Qt::UserRole).toInt();
}

void VehiclesWidget::reload()
{
    const int previous = selectedId();
    m_table->setRowCount(0);
    if (!m_db || !m_db->isOpen())
        return;

    for (const Vehicle& v : m_db->vehicles()) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* vinItem = new QTableWidgetItem(v.vin);
        vinItem->setData(Qt::UserRole, v.id);
        m_table->setItem(row, 0, vinItem);
        m_table->setItem(row, 1, new QTableWidgetItem(v.brand));
        m_table->setItem(row, 2, new QTableWidgetItem(v.model));
        m_table->setItem(row, 3, new QTableWidgetItem(v.engine));
        m_table->setItem(row, 4, new QTableWidgetItem(v.registration));
        m_table->setItem(row, 5, new QTableWidgetItem(v.year > 0 ? QString::number(v.year) : QString()));
        m_table->setItem(row, 6, new QTableWidgetItem(v.mileage > 0 ? QStringLiteral("%L1 km").arg(v.mileage) : QString()));

        if (v.id == previous)
            m_table->selectRow(row);
    }
    updateDetails();
}

void VehiclesWidget::updateDetails()
{
    const int id = selectedId();
    if (id <= 0 || !m_db) {
        m_details->setText(tr("<i>Sélectionnez un véhicule pour afficher ses statistiques.</i>"));
        return;
    }

    const Vehicle v = m_db->vehicle(id);
    const QList<Trip> trips = m_db->trips(id);
    const QList<SessionInfo> sessions = m_db->sessions(id);

    double distance = 0, vmax = 0, speedSum = 0;
    for (const Trip& t : trips) {
        distance += t.distance;
        vmax = qMax(vmax, t.maxSpeed);
        speedSum += t.avgSpeed;
    }

    qint64 seconds = 0;
    int samples = 0;
    for (const SessionInfo& s : sessions) {
        if (s.start.isValid() && s.end.isValid())
            seconds += s.start.secsTo(s.end);
        samples += s.sampleCount;
    }

    m_details->setText(tr("<b>%1</b><br>"
                          "Sessions : %2 · Mesures enregistrées : %3 · Temps moteur suivi : %4 h<br>"
                          "Trajets : %5 · Distance totale : %6 km · Vitesse moyenne : %7 km/h · Vitesse max : %8 km/h<br>"
                          "Derniers défauts lus : %9")
                           .arg(v.displayName().toHtmlEscaped())
                           .arg(sessions.size())
                           .arg(samples)
                           .arg(seconds / 3600.0, 0, 'f', 1)
                           .arg(trips.size())
                           .arg(distance, 0, 'f', 1)
                           .arg(trips.isEmpty() ? 0.0 : speedSum / trips.size(), 0, 'f', 1)
                           .arg(vmax, 0, 'f', 0)
                           .arg(m_db->lastDtcs(id).size()));
}

void VehiclesWidget::addVehicle()
{
    VehicleDialog dlg(Vehicle(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    Vehicle v = dlg.vehicle();
    if (m_db->vehicleByVin(v.vin).isValid()) {
        QMessageBox::warning(this, tr("Véhicule existant"), tr("Un véhicule avec ce VIN existe déjà."));
        return;
    }
    if (!m_db->saveVehicle(v))
        QMessageBox::critical(this, tr("Erreur"), m_db->lastError());
    reload();
}

void VehiclesWidget::editVehicle()
{
    const int id = selectedId();
    if (id <= 0)
        return;

    VehicleDialog dlg(m_db->vehicle(id), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    Vehicle v = dlg.vehicle();
    const Vehicle other = m_db->vehicleByVin(v.vin);
    if (other.isValid() && other.id != v.id) {
        QMessageBox::warning(this, tr("Véhicule existant"), tr("Un autre véhicule utilise déjà ce VIN."));
        return;
    }
    if (!m_db->saveVehicle(v))
        QMessageBox::critical(this, tr("Erreur"), m_db->lastError());
    reload();
}

void VehiclesWidget::deleteVehicle()
{
    const int id = selectedId();
    if (id <= 0)
        return;

    const Vehicle v = m_db->vehicle(id);
    const auto answer = QMessageBox::warning(
        this, tr("Supprimer le véhicule"),
        tr("Supprimer « %1 » ainsi que toutes ses sessions, mesures, défauts et trajets ?\n"
           "Cette action est irréversible (pensez à faire une sauvegarde).").arg(v.displayName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::Yes && !m_db->deleteVehicle(id))
        QMessageBox::critical(this, tr("Erreur"), m_db->lastError());
    reload();
}
