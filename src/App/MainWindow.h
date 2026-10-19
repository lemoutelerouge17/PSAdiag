#pragma once

#include "Alerts/AlarmManager.h"
#include "Database/BackupManager.h"
#include "Database/DatabaseManager.h"
#include "OBD/ObdEngine.h"
#include "Trips/TripManager.h"

#include <QMainWindow>
#include <QTimer>

class ConnectionWidget;
class DashboardWidget;
class DTCWidget;
class GraphWidget;
class HistoryWidget;
class LogWidget;
class QAction;
class QComboBox;
class QLabel;
class QTabWidget;
class StatisticsWidget;
class TripsWidget;
class VehiclesWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    // Construction de l'interface
    void createWidgets();
    void createMenus();
    void createToolBar();
    void connectSignals();
    void openDatabase();

    // Moteur OBD
    void onConnectRequested(const QString& port, qint32 baud, bool simulate);
    void onStateChanged(ObdEngine::State state);
    void onAdapterReady(const QString& identity, const QString& protocol);
    void onVinReceived(const QString& vin);
    void onDtcsReceived(const QList<DTC>& dtcs);
    void onSample(const VehicleSample& sample);
    void onEngineError(const QString& message);

    // Acquisition, trajets, alarmes
    void toggleAcquisition();
    void startTrip();
    void stopTrip();
    void onAlarm(const QString& key, const QString& title, const QString& message,
                 double threshold, double measured);

    // Véhicules
    void setActiveVehicle(int vehicleId);
    void reloadVehicleFilter();
    void reloadTrips();

    // Exports
    void exportCsv(const QList<VehicleSample>& samples);
    void exportPdf(const QList<VehicleSample>& samples, const QString& sessionLabel);
    void exportExcel(const QList<VehicleSample>& samples);
    void exportDtcCsv();
    void exportTripsCsv();
    QString askSaveFile(const QString& title, const QString& baseName, const QString& filter,
                        const QString& extension);
    QList<VehicleSample> exportSource(QString* label) const;

    // Base de données
    void backupNow();
    void exportDatabase();
    void importDatabase();
    void restoreBackup();
    void autoBackupIfNeeded();

    // Divers
    void applyTheme(bool dark);
    void editAlarmSettings();
    void editPollInterval();
    void showAbout();
    void updateActions();
    void refreshStatistics();
    void status(const QString& message, int timeoutMs = 5000);

    ObdEngine m_engine;
    DatabaseManager m_db;
    BackupManager* m_backup = nullptr;
    AlarmManager m_alarms;
    TripManager m_trips;

    // Widgets
    QTabWidget* m_tabs = nullptr;
    ConnectionWidget* m_connection = nullptr;
    DashboardWidget* m_dashboard = nullptr;
    GraphWidget* m_graph = nullptr;
    DTCWidget* m_dtcWidget = nullptr;
    HistoryWidget* m_history = nullptr;
    StatisticsWidget* m_statistics = nullptr;
    TripsWidget* m_tripsWidget = nullptr;
    VehiclesWidget* m_vehicles = nullptr;
    LogWidget* m_log = nullptr;
    QComboBox* m_vehicleFilter = nullptr;
    QLabel* m_statusSamples = nullptr;

    // Actions
    QAction* m_acquireAction = nullptr;
    QAction* m_vinAction = nullptr;
    QAction* m_dtcAction = nullptr;
    QAction* m_tripAction = nullptr;
    QAction* m_darkAction = nullptr;
    QAction* m_lightAction = nullptr;

    // État de la session
    int m_sessionId = -1;
    int m_vehicleId = -1;
    QString m_vin;
    QList<VehicleSample> m_liveSamples;
    int m_recordedCount = 0;
    QTimer m_backupTimer;
    QTimer m_tripRefresh;
};
