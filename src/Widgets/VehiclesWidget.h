#pragma once

#include <QWidget>

class DatabaseManager;
class QLabel;
class QTableWidget;

// Gestion multi-véhicules : liste, création, modification, suppression,
// statistiques cumulées par véhicule.
class VehiclesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VehiclesWidget(DatabaseManager* db, QWidget* parent = nullptr);

public slots:
    void reload();

signals:
    void activateRequested(int vehicleId);

private:
    int selectedId() const;
    void addVehicle();
    void editVehicle();
    void deleteVehicle();
    void updateDetails();

    DatabaseManager* m_db;
    QTableWidget* m_table;
    QLabel* m_details;
};
