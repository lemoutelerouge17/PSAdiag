#pragma once

#include "Models/VehicleSample.h"

#include <QWidget>

class DatabaseManager;
class QComboBox;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;
class SampleTableModel;

// Historique SQLite : choix d'une session, tableau filtrable des mesures,
// relecture dans le graphique et exports.
class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(DatabaseManager* db, QWidget* parent = nullptr);

    int currentSessionId() const;
    QString currentSessionLabel() const;
    QList<VehicleSample> currentSamples() const;

public slots:
    void reloadSessions();
    void setVehicleFilter(int vehicleId);

signals:
    void replayRequested(const QList<VehicleSample>& samples);
    void exportCsvRequested();
    void exportPdfRequested();
    void exportExcelRequested();

private:
    void loadSession();

    DatabaseManager* m_db;
    int m_vehicleFilter = -1;

    QComboBox* m_sessionCombo;
    QLineEdit* m_filter;
    QTableView* m_table;
    SampleTableModel* m_model;
    QSortFilterProxyModel* m_proxy;
    QLabel* m_info;
};
