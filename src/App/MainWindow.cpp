#include "MainWindow.h"

#include "Core/AppInfo.h"
#include "Core/Logger.h"
#include "Export/CsvExporter.h"
#include "Export/ExcelExporter.h"
#include "Export/PdfExporter.h"
#include "Utils/ThemeManager.h"
#include "Widgets/AlarmSettingsDialog.h"
#include "Widgets/ConnectionWidget.h"
#include "Widgets/DashboardWidget.h"
#include "Widgets/DTCWidget.h"
#include "Widgets/GraphWidget.h"
#include "Widgets/HistoryWidget.h"
#include "Widgets/LogWidget.h"
#include "Widgets/StatisticsWidget.h"
#include "Widgets/TripsWidget.h"
#include "Widgets/VehicleDialog.h"
#include "Widgets/VehiclesWidget.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int kMaxLiveSamples = 100000;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 %2 – Diagnostic OBD-II").arg(AppInfo::name(), AppInfo::version()));
    resize(1400, 900);

    m_backup = new BackupManager(&m_db, this);

    openDatabase();
    createWidgets();
    createMenus();
    createToolBar();
    connectSignals();

    QSettings cfg;
    restoreGeometry(cfg.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(cfg.value(QStringLiteral("window/state")).toByteArray());
    m_engine.setPollInterval(cfg.value(QStringLiteral("acquisition/interval"), 500).toInt());

    applyTheme(ThemeManager::current() == ThemeManager::Theme::Dark);

    // Sauvegarde automatique quotidienne (+ au démarrage si la dernière a plus de 24 h).
    m_backupTimer.setInterval(24 * 60 * 60 * 1000);
    connect(&m_backupTimer, &QTimer::timeout, this, [this]() { m_backup->createBackup(); });
    m_backupTimer.start();
    QTimer::singleShot(3000, this, &MainWindow::autoBackupIfNeeded);

    // Rafraîchissement de la durée du trajet en cours.
    m_tripRefresh.setInterval(1000);
    connect(&m_tripRefresh, &QTimer::timeout, this, [this]() {
        if (m_trips.isRunning())
            m_tripsWidget->setLiveTrip(m_trips.current(), m_trips.durationSeconds());
    });

    updateActions();
    Logger::info(tr("%1 %2 démarré – données : %3").arg(AppInfo::name(), AppInfo::version(), AppInfo::dataDir()));
}

MainWindow::~MainWindow()
{
    m_db.endSession(m_sessionId);
}

// ===========================================================================
// Construction
// ===========================================================================

void MainWindow::openDatabase()
{
    if (!m_db.open()) {
        QMessageBox::critical(this, tr("Base de données"),
                              tr("Impossible d'ouvrir la base de données :\n%1\n\n"
                                 "L'application fonctionnera sans historique.")
                                  .arg(m_db.lastError()));
    }
}

void MainWindow::createWidgets()
{
    m_connection = new ConnectionWidget;
    m_dashboard = new DashboardWidget;
    m_graph = new GraphWidget;
    m_dtcWidget = new DTCWidget;
    m_history = new HistoryWidget(&m_db);
    m_statistics = new StatisticsWidget;
    m_tripsWidget = new TripsWidget;
    m_vehicles = new VehiclesWidget(&m_db);
    m_log = new LogWidget;

    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->addTab(m_dashboard, tr("Tableau de bord"));
    m_tabs->addTab(m_graph, tr("Graphiques"));
    m_tabs->addTab(m_dtcWidget, tr("Défauts (DTC)"));
    m_tabs->addTab(m_history, tr("Historique"));
    m_tabs->addTab(m_statistics, tr("Statistiques"));
    m_tabs->addTab(m_tripsWidget, tr("Trajets"));
    m_tabs->addTab(m_vehicles, tr("Véhicules"));
    m_tabs->addTab(m_log, tr("Journal"));

    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_connection);
    layout->addWidget(m_tabs, 1);
    setCentralWidget(central);

    m_statusSamples = new QLabel;
    statusBar()->addPermanentWidget(m_statusSamples);

    reloadTrips();
}

void MainWindow::createMenus()
{
    // --- Fichier ------------------------------------------------------------
    QMenu* file = menuBar()->addMenu(tr("&Fichier"));

    file->addAction(tr("Exporter les mesures en CSV…"), this, [this]() {
        QString label;
        exportCsv(exportSource(&label));
    });
    file->addAction(tr("Exporter un rapport PDF…"), this, [this]() {
        QString label;
        const auto samples = exportSource(&label);
        exportPdf(samples, label);
    });
    QAction* xlsx = file->addAction(tr("Exporter en Excel…"), this, [this]() {
        QString label;
        exportExcel(exportSource(&label));
    });
    xlsx->setVisible(ExcelExporter::isAvailable());

    file->addSeparator();
    file->addAction(tr("Sauvegarder la base maintenant"), this, &MainWindow::backupNow);
    file->addAction(tr("Restaurer une sauvegarde…"), this, &MainWindow::restoreBackup);
    file->addAction(tr("Exporter la base…"), this, &MainWindow::exportDatabase);
    file->addAction(tr("Importer une base…"), this, &MainWindow::importDatabase);
    file->addAction(tr("Ouvrir le dossier des données"), this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppInfo::dataDir()));
    });
    file->addSeparator();
    QAction* quit = file->addAction(tr("&Quitter"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    // --- Diagnostic ---------------------------------------------------------
    QMenu* diag = menuBar()->addMenu(tr("&Diagnostic"));
    m_acquireAction = diag->addAction(tr("Démarrer l'acquisition"), this, &MainWindow::toggleAcquisition);
    m_acquireAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_vinAction = diag->addAction(tr("Lire le VIN"), this, [this]() { m_engine.requestVin(); });
    m_dtcAction = diag->addAction(tr("Lire les défauts"), this, [this]() { m_engine.requestDtcs(); });
    m_dtcAction->setShortcut(QKeySequence(Qt::Key_F6));
    m_tripAction = diag->addAction(tr("Démarrer un trajet"), this, [this]() {
        if (m_trips.isRunning()) stopTrip(); else startTrip();
    });
    diag->addSeparator();
    diag->addAction(tr("Intervalle d'acquisition…"), this, &MainWindow::editPollInterval);
    diag->addAction(tr("Seuils d'alarme…"), this, &MainWindow::editAlarmSettings);

    // --- Affichage ----------------------------------------------------------
    QMenu* view = menuBar()->addMenu(tr("&Affichage"));
    auto* themes = new QActionGroup(this);
    m_darkAction = view->addAction(tr("Thème sombre"));
    m_lightAction = view->addAction(tr("Thème clair"));
    for (QAction* a : { m_darkAction, m_lightAction }) {
        a->setCheckable(true);
        themes->addAction(a);
    }
    connect(m_darkAction, &QAction::triggered, this, [this]() { applyTheme(true); });
    connect(m_lightAction, &QAction::triggered, this, [this]() { applyTheme(false); });

    // --- Aide ---------------------------------------------------------------
    QMenu* help = menuBar()->addMenu(tr("&Aide"));
    help->addAction(tr("À propos de %1").arg(AppInfo::name()), this, &MainWindow::showAbout);
    help->addAction(tr("À propos de Qt"), this, []() { QApplication::aboutQt(); });
}

void MainWindow::createToolBar()
{
    QToolBar* bar = addToolBar(tr("Diagnostic"));
    bar->setObjectName(QStringLiteral("mainToolBar"));
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    bar->addAction(m_acquireAction);
    bar->addAction(m_vinAction);
    bar->addAction(m_dtcAction);
    bar->addAction(m_tripAction);
    bar->addSeparator();

    bar->addWidget(new QLabel(tr("  Véhicule (historique) : ")));
    m_vehicleFilter = new QComboBox;
    m_vehicleFilter->setMinimumWidth(220);
    bar->addWidget(m_vehicleFilter);
    reloadVehicleFilter();

    connect(m_vehicleFilter, &QComboBox::currentIndexChanged, this, [this]() {
        m_history->setVehicleFilter(m_vehicleFilter->currentData().toInt());
        reloadTrips();
    });
}

void MainWindow::connectSignals()
{
    // Connexion
    connect(m_connection, &ConnectionWidget::connectRequested, this, &MainWindow::onConnectRequested);
    connect(m_connection, &ConnectionWidget::disconnectRequested, this, [this]() { m_engine.disconnectAdapter(); });

    // Moteur OBD
    connect(&m_engine, &ObdEngine::stateChanged, this, &MainWindow::onStateChanged);
    connect(&m_engine, &ObdEngine::adapterReady, this, &MainWindow::onAdapterReady);
    connect(&m_engine, &ObdEngine::vinReceived, this, &MainWindow::onVinReceived);
    connect(&m_engine, &ObdEngine::dtcsReceived, this, &MainWindow::onDtcsReceived);
    connect(&m_engine, &ObdEngine::sampleReady, this, &MainWindow::onSample);
    connect(&m_engine, &ObdEngine::errorOccurred, this, &MainWindow::onEngineError);
    connect(&m_engine, &ObdEngine::pollingChanged, this, [this](bool polling) {
        m_dashboard->setRecording(polling);
        updateActions();
    });
    connect(&m_engine, &ObdEngine::dtcsCleared, this, [this](bool ok) {
        if (ok)
            status(tr("Codes défauts effacés"));
        else
            QMessageBox::warning(this, tr("Effacement des défauts"),
                                 tr("Le calculateur a refusé l'effacement (moteur tournant ?)."));
    });

    // Défauts
    connect(m_dtcWidget, &DTCWidget::readRequested, &m_engine, &ObdEngine::requestDtcs);
    connect(m_dtcWidget, &DTCWidget::clearRequested, &m_engine, &ObdEngine::clearDtcs);
    connect(m_dtcWidget, &DTCWidget::exportRequested, this, &MainWindow::exportDtcCsv);

    // Historique
    connect(m_history, &HistoryWidget::replayRequested, this, [this](const QList<VehicleSample>& samples) {
        if (m_engine.isPolling()) {
            QMessageBox::information(this, tr("Relecture"),
                                     tr("Arrêtez l'acquisition avant de relire une session."));
            return;
        }
        m_graph->loadSamples(samples);
        m_tabs->setCurrentWidget(m_graph);
    });
    connect(m_history, &HistoryWidget::exportCsvRequested, this, [this]() {
        exportCsv(m_history->currentSamples());
    });
    connect(m_history, &HistoryWidget::exportPdfRequested, this, [this]() {
        exportPdf(m_history->currentSamples(), m_history->currentSessionLabel());
    });
    connect(m_history, &HistoryWidget::exportExcelRequested, this, [this]() {
        exportExcel(m_history->currentSamples());
    });

    // Statistiques
    connect(m_statistics, &StatisticsWidget::refreshRequested, this, &MainWindow::refreshStatistics);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this]() {
        if (m_tabs->currentWidget() == m_statistics)
            refreshStatistics();
    });

    // Trajets
    connect(m_tripsWidget, &TripsWidget::startRequested, this, &MainWindow::startTrip);
    connect(m_tripsWidget, &TripsWidget::stopRequested, this, &MainWindow::stopTrip);
    connect(m_tripsWidget, &TripsWidget::exportRequested, this, &MainWindow::exportTripsCsv);
    connect(m_tripsWidget, &TripsWidget::deleteRequested, this, [this](int id) { m_db.deleteTrip(id); });
    connect(&m_trips, &TripManager::tripUpdated, this, [this](const Trip& t) {
        m_dashboard->setTripInfo(tr("%1 km · %2 · moy. %3 km/h · max %4 km/h")
                                     .arg(t.distance, 0, 'f', 2)
                                     .arg(TripsWidget::formatDuration(m_trips.durationSeconds()))
                                     .arg(t.avgSpeed, 0, 'f', 0)
                                     .arg(t.maxSpeed, 0, 'f', 0));
    });

    // Véhicules
    connect(m_vehicles, &VehiclesWidget::activateRequested, this, &MainWindow::setActiveVehicle);
    connect(&m_db, &DatabaseManager::vehiclesChanged, this, [this]() {
        m_vehicles->reload();
        reloadVehicleFilter();
        if (m_vehicleId > 0)
            m_dashboard->setVehicleName(m_db.vehicle(m_vehicleId).displayName());
    });
    connect(&m_db, &DatabaseManager::sessionsChanged, m_history, &HistoryWidget::reloadSessions);
    connect(&m_db, &DatabaseManager::tripsChanged, this, &MainWindow::reloadTrips);

    // Alarmes
    connect(&m_alarms, &AlarmManager::alarmTriggered, this, &MainWindow::onAlarm);

    // Base restaurée : tout recharger.
    connect(m_backup, &BackupManager::databaseRestored, this, [this]() {
        m_sessionId = -1;
        m_vehicleId = -1;
        m_vehicles->reload();
        reloadVehicleFilter();
        m_history->reloadSessions();
        reloadTrips();
    });

    // Terminal
    connect(m_log, &LogWidget::commandEntered, &m_engine, &ObdEngine::sendRaw);
}

// ===========================================================================
// Moteur OBD
// ===========================================================================

void MainWindow::onConnectRequested(const QString& port, qint32 baud, bool simulate)
{
    m_liveSamples.clear();
    m_recordedCount = 0;
    m_vehicleId = -1;   // sera déterminé par la lecture du VIN
    m_graph->clear();
    m_dashboard->clearValues();
    m_dtcWidget->setDtcs({});
    m_dashboard->setDtcCount(-1);

    // En cas d'échec, le message détaillé arrive par ObdEngine::errorOccurred.
    if (simulate)
        m_engine.connectSimulator();
    else
        m_engine.connectSerial(port, baud);
}

void MainWindow::onStateChanged(ObdEngine::State state)
{
    m_connection->setEngineState(state);

    switch (state) {
    case ObdEngine::State::Disconnected:
        m_dashboard->setConnectionState(tr("Déconnecté"), false);
        if (m_trips.isRunning())
            stopTrip();
        m_db.endSession(m_sessionId);
        m_sessionId = -1;
        m_dashboard->setRecording(false);
        status(tr("Adaptateur déconnecté"));
        break;
    case ObdEngine::State::Initializing:
        m_dashboard->setConnectionState(tr("Initialisation…"), false);
        break;
    case ObdEngine::State::Ready:
        break;
    }

    m_dtcWidget->setAdapterReady(state == ObdEngine::State::Ready);
    m_log->setTerminalEnabled(state != ObdEngine::State::Disconnected);
    updateActions();
}

void MainWindow::onAdapterReady(const QString& identity, const QString& protocol)
{
    m_connection->setAdapterInfo(identity, protocol);
    m_dashboard->setConnectionState(identity, true);

    m_vin.clear();
    m_dashboard->setVin(QString());
    m_dashboard->setVehicleName(QString());

    // Une session par connexion ; le véhicule est associé dès que le VIN est lu.
    m_sessionId = m_db.isOpen() ? m_db.startSession(m_vehicleId, identity, protocol) : -1;

    status(tr("Adaptateur prêt – lecture du VIN et des défauts…"));
    m_engine.requestVin();
    m_engine.requestDtcs();
}

void MainWindow::onVinReceived(const QString& vin)
{
    m_vin = vin;
    m_dashboard->setVin(vin);

    if (!m_db.isOpen())
        return;

    const bool known = m_db.vehicleByVin(vin).isValid();
    const int id = m_db.ensureVehicle(vin);
    setActiveVehicle(id);

    if (!known) {
        status(tr("Nouveau véhicule enregistré (VIN %1)").arg(vin));
        const auto answer = QMessageBox::question(
            this, tr("Nouveau véhicule"),
            tr("Nouveau véhicule détecté (VIN %1).\nVoulez-vous compléter sa fiche "
               "(marque, modèle, immatriculation…) ?").arg(vin));
        if (answer == QMessageBox::Yes) {
            VehicleDialog dlg(m_db.vehicle(id), this);
            if (dlg.exec() == QDialog::Accepted) {
                Vehicle v = dlg.vehicle();
                m_db.saveVehicle(v);
            }
        }
    }
}

void MainWindow::onDtcsReceived(const QList<DTC>& dtcs)
{
    m_dtcWidget->setDtcs(dtcs, tr("(lus le %1)").arg(QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy à HH:mm"))));

    int stored = 0;
    for (const DTC& d : dtcs)
        if (d.status == DTC::Status::Stored)
            ++stored;
    m_dashboard->setDtcCount(stored);

    if (m_db.isOpen()) {
        for (const DTC& d : dtcs)
            m_db.addDtc(m_vehicleId, m_sessionId, d);
    }

    status(dtcs.isEmpty() ? tr("Aucun code défaut") : tr("%1 code(s) défaut lu(s)").arg(dtcs.size()));
    Logger::info(tr("Lecture défauts : %1").arg(dtcs.isEmpty() ? tr("aucun") : [&]() {
        QStringList codes;
        for (const DTC& d : dtcs)
            codes << d.code;
        return codes.join(QStringLiteral(", "));
    }()));
}

void MainWindow::onSample(const VehicleSample& sample)
{
    m_dashboard->updateSample(sample);
    m_graph->addSample(sample);
    m_alarms.check(sample);
    m_trips.update(sample);

    m_liveSamples.append(sample);
    if (m_liveSamples.size() > kMaxLiveSamples)
        m_liveSamples.removeFirst();

    if (m_sessionId > 0 && m_db.addSample(m_sessionId, sample))
        ++m_recordedCount;

    m_statusSamples->setText(tr("%1 mesures enregistrées").arg(m_recordedCount));
}

void MainWindow::onEngineError(const QString& message)
{
    Logger::warning(message);
    status(message, 8000);

    // Erreurs de connexion / d'initialisation : on prévient explicitement l'utilisateur.
    if (m_engine.state() != ObdEngine::State::Ready)
        QMessageBox::warning(this, tr("Adaptateur OBD"), message);
}

// ===========================================================================
// Acquisition, trajets, alarmes
// ===========================================================================

void MainWindow::toggleAcquisition()
{
    if (m_engine.isPolling()) {
        m_engine.stopPolling();
        status(tr("Acquisition arrêtée"));
    } else {
        m_engine.startPolling();
        m_tabs->setCurrentWidget(m_dashboard);
        status(tr("Acquisition démarrée (intervalle %1 ms)").arg(m_engine.pollInterval()));
    }
}

void MainWindow::startTrip()
{
    if (!m_engine.isPolling()) {
        if (!m_engine.isReady()) {
            QMessageBox::information(this, tr("Trajet"), tr("Connectez d'abord l'adaptateur OBD."));
            return;
        }
        m_engine.startPolling();
    }
    m_trips.startTrip(m_vehicleId);
    m_tripsWidget->setRunning(true);
    m_tripRefresh.start();
    updateActions();
}

void MainWindow::stopTrip()
{
    if (!m_trips.isRunning())
        return;

    Trip trip = m_trips.stopTrip();
    m_tripRefresh.stop();
    m_tripsWidget->setRunning(false);
    m_dashboard->setTripInfo(tr("Dernier trajet : %1 km, moy. %2 km/h")
                                 .arg(trip.distance, 0, 'f', 2)
                                 .arg(trip.avgSpeed, 0, 'f', 0));

    if (m_db.isOpen() && m_db.saveTrip(trip))
        status(tr("Trajet enregistré : %1 km").arg(trip.distance, 0, 'f', 2));
    updateActions();
}

void MainWindow::onAlarm(const QString& key, const QString& title, const QString& message,
                         double threshold, double measured)
{
    Logger::warning(tr("ALARME %1").arg(message));
    status(tr("⚠ %1").arg(message), 10000);

    if (m_db.isOpen())
        m_db.addAlarm(m_vehicleId, key, threshold, measured, message);

    if (m_alarms.popupsEnabled()) {
        // Fenêtre non modale : l'acquisition continue.
        auto* box = new QMessageBox(QMessageBox::Warning, tr("Alarme – %1").arg(title), message,
                                    QMessageBox::Ok, this);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setModal(false);
        box->show();
    }
    QApplication::beep();
}

// ===========================================================================
// Véhicules
// ===========================================================================

void MainWindow::setActiveVehicle(int vehicleId)
{
    if (vehicleId <= 0)
        return;

    m_vehicleId = vehicleId;
    const Vehicle v = m_db.vehicle(vehicleId);
    m_dashboard->setVehicleName(v.displayName());
    if (m_vin.isEmpty())
        m_dashboard->setVin(v.vin);

    if (m_sessionId > 0)
        m_db.setSessionVehicle(m_sessionId, vehicleId);
    if (m_trips.isRunning())
        m_trips.setVehicle(vehicleId);

    status(tr("Véhicule actif : %1").arg(v.displayName()));
}

void MainWindow::reloadVehicleFilter()
{
    if (!m_vehicleFilter)
        return;

    const int previous = m_vehicleFilter->currentData().isValid() ? m_vehicleFilter->currentData().toInt() : -1;
    m_vehicleFilter->blockSignals(true);
    m_vehicleFilter->clear();
    m_vehicleFilter->addItem(tr("Tous les véhicules"), -1);
    if (m_db.isOpen()) {
        for (const Vehicle& v : m_db.vehicles())
            m_vehicleFilter->addItem(v.displayName(), v.id);
    }
    const int idx = m_vehicleFilter->findData(previous);
    m_vehicleFilter->setCurrentIndex(idx >= 0 ? idx : 0);
    m_vehicleFilter->blockSignals(false);
}

void MainWindow::reloadTrips()
{
    if (!m_tripsWidget)
        return;
    const int filter = m_vehicleFilter ? m_vehicleFilter->currentData().toInt() : -1;
    m_tripsWidget->setTrips(m_db.isOpen() ? m_db.trips(filter) : QList<Trip>());
}

// ===========================================================================
// Exports
// ===========================================================================

QList<VehicleSample> MainWindow::exportSource(QString* label) const
{
    // Session en cours si elle contient des mesures, sinon session sélectionnée dans l'historique.
    if (!m_liveSamples.isEmpty()) {
        *label = tr("Session en cours");
        return m_liveSamples;
    }
    *label = m_history->currentSessionLabel();
    return m_history->currentSamples();
}

QString MainWindow::askSaveFile(const QString& title, const QString& baseName,
                                const QString& filter, const QString& extension)
{
    QSettings cfg;
    const QString dir = cfg.value(QStringLiteral("export/dir"),
                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    const QString suggested = QStringLiteral("%1/%2_%3.%4")
                                  .arg(dir, baseName,
                                       QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")),
                                       extension);

    QString file = QFileDialog::getSaveFileName(this, title, suggested, filter);
    if (file.isEmpty())
        return QString();
    if (!file.endsWith(QLatin1Char('.') + extension, Qt::CaseInsensitive))
        file += QLatin1Char('.') + extension;

    cfg.setValue(QStringLiteral("export/dir"), QFileInfo(file).absolutePath());
    return file;
}

void MainWindow::exportCsv(const QList<VehicleSample>& samples)
{
    if (samples.isEmpty()) {
        QMessageBox::information(this, tr("Export CSV"), tr("Aucune mesure à exporter."));
        return;
    }
    const QString file = askSaveFile(tr("Exporter les mesures"), QStringLiteral("mesures"),
                                     tr("Fichier CSV (*.csv)"), QStringLiteral("csv"));
    if (file.isEmpty())
        return;

    QString error;
    if (CsvExporter::exportHistory(file, samples, &error)) {
        m_db.logExport(QStringLiteral("csv"), file);
        status(tr("Export CSV : %1").arg(file));
    } else {
        QMessageBox::critical(this, tr("Export CSV"), error);
    }
}

void MainWindow::exportPdf(const QList<VehicleSample>& samples, const QString& sessionLabel)
{
    const QString file = askSaveFile(tr("Exporter un rapport PDF"), QStringLiteral("rapport"),
                                     tr("Document PDF (*.pdf)"), QStringLiteral("pdf"));
    if (file.isEmpty())
        return;

    ReportData data;
    data.vehicle = m_vehicleId > 0 ? m_db.vehicle(m_vehicleId) : Vehicle();
    if (data.vehicle.vin.isEmpty())
        data.vehicle.vin = m_vin;
    data.adapter = m_engine.adapterIdentity();
    data.protocol = m_engine.protocolName();
    data.sessionLabel = sessionLabel;
    data.samples = samples;
    data.dtcs = m_dtcWidget->dtcs();

    // Le graphique reflète la session exportée.
    if (!samples.isEmpty()) {
        if (samples.constData() != m_liveSamples.constData() && !m_engine.isPolling())
            m_graph->loadSamples(samples);
        data.chart = m_graph->capture();
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfExporter::exportReport(file, data, &error);
    QApplication::restoreOverrideCursor();

    if (ok) {
        m_db.logExport(QStringLiteral("pdf"), file);
        status(tr("Rapport PDF : %1").arg(file));
        QDesktopServices::openUrl(QUrl::fromLocalFile(file));
    } else {
        QMessageBox::critical(this, tr("Export PDF"), error);
    }
}

void MainWindow::exportExcel(const QList<VehicleSample>& samples)
{
    const QString file = askSaveFile(tr("Exporter en Excel"), QStringLiteral("diagnostic"),
                                     tr("Classeur Excel (*.xlsx)"), QStringLiteral("xlsx"));
    if (file.isEmpty())
        return;

    QString error;
    const int filter = m_vehicleFilter ? m_vehicleFilter->currentData().toInt() : -1;
    if (ExcelExporter::exportWorkbook(file, samples, m_dtcWidget->dtcs(), m_db.trips(filter), &error)) {
        m_db.logExport(QStringLiteral("xlsx"), file);
        status(tr("Export Excel : %1").arg(file));
    } else {
        QMessageBox::critical(this, tr("Export Excel"), error);
    }
}

void MainWindow::exportDtcCsv()
{
    const QString file = askSaveFile(tr("Exporter les défauts"), QStringLiteral("defauts"),
                                     tr("Fichier CSV (*.csv)"), QStringLiteral("csv"));
    if (file.isEmpty())
        return;
    QString error;
    if (CsvExporter::exportDtcs(file, m_dtcWidget->dtcs(), &error)) {
        m_db.logExport(QStringLiteral("dtc_csv"), file);
        status(tr("Export des défauts : %1").arg(file));
    } else {
        QMessageBox::critical(this, tr("Export"), error);
    }
}

void MainWindow::exportTripsCsv()
{
    const QList<Trip> trips = m_tripsWidget->trips();
    if (trips.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("Aucun trajet à exporter."));
        return;
    }
    const QString file = askSaveFile(tr("Exporter les trajets"), QStringLiteral("trajets"),
                                     tr("Fichier CSV (*.csv)"), QStringLiteral("csv"));
    if (file.isEmpty())
        return;
    QString error;
    if (CsvExporter::exportTrips(file, trips, &error)) {
        m_db.logExport(QStringLiteral("trips_csv"), file);
        status(tr("Export des trajets : %1").arg(file));
    } else {
        QMessageBox::critical(this, tr("Export"), error);
    }
}

// ===========================================================================
// Base de données
// ===========================================================================

void MainWindow::backupNow()
{
    const QString path = m_backup->createBackup();
    if (path.isEmpty())
        QMessageBox::critical(this, tr("Sauvegarde"), m_backup->lastError());
    else
        status(tr("Sauvegarde créée : %1").arg(path));
}

void MainWindow::autoBackupIfNeeded()
{
    const QStringList list = m_backup->backups();
    if (!list.isEmpty()) {
        const QDateTime last = QFileInfo(list.first()).lastModified();
        if (last.secsTo(QDateTime::currentDateTime()) < 24 * 3600)
            return;
    }
    m_backup->createBackup();
}

void MainWindow::exportDatabase()
{
    const QString file = askSaveFile(tr("Exporter la base"), QStringLiteral("psadiag"),
                                     tr("Base SQLite (*.db)"), QStringLiteral("db"));
    if (file.isEmpty())
        return;
    if (m_backup->createBackup(file).isEmpty())
        QMessageBox::critical(this, tr("Export de la base"), m_backup->lastError());
    else
        status(tr("Base exportée : %1").arg(file));
}

void MainWindow::importDatabase()
{
    if (m_engine.state() != ObdEngine::State::Disconnected) {
        QMessageBox::information(this, tr("Import"), tr("Déconnectez l'adaptateur avant d'importer une base."));
        return;
    }
    const QString file = QFileDialog::getOpenFileName(this, tr("Importer une base"),
                                                      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      tr("Base SQLite (*.db)"));
    if (file.isEmpty())
        return;

    if (QMessageBox::warning(this, tr("Importer une base"),
                             tr("La base actuelle sera remplacée (une sauvegarde est faite automatiquement).\n"
                                "Continuer ?"),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (m_backup->restoreBackup(file))
        status(tr("Base importée"));
    else
        QMessageBox::critical(this, tr("Import"), m_backup->lastError());
}

void MainWindow::restoreBackup()
{
    if (m_engine.state() != ObdEngine::State::Disconnected) {
        QMessageBox::information(this, tr("Restauration"), tr("Déconnectez l'adaptateur avant de restaurer."));
        return;
    }

    const QStringList list = m_backup->backups();
    if (list.isEmpty()) {
        QMessageBox::information(this, tr("Restauration"), tr("Aucune sauvegarde disponible."));
        return;
    }

    QStringList labels;
    for (const QString& f : list) {
        const QFileInfo fi(f);
        labels << QStringLiteral("%1  (%2 Ko)")
                      .arg(fi.lastModified().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")))
                      .arg(fi.size() / 1024);
    }

    bool ok = false;
    const QString choice = QInputDialog::getItem(this, tr("Restaurer une sauvegarde"),
                                                 tr("Sauvegarde à restaurer :"), labels, 0, false, &ok);
    if (!ok)
        return;

    const QString file = list.at(labels.indexOf(choice));
    if (m_backup->restoreBackup(file))
        status(tr("Base restaurée"));
    else
        QMessageBox::critical(this, tr("Restauration"), m_backup->lastError());
}

// ===========================================================================
// Divers
// ===========================================================================

void MainWindow::applyTheme(bool dark)
{
    const auto theme = dark ? ThemeManager::Theme::Dark : ThemeManager::Theme::Light;
    ThemeManager::apply(theme);
    ThemeManager::save(theme);
    m_graph->applyTheme(dark);
    m_darkAction->setChecked(dark);
    m_lightAction->setChecked(!dark);
}

void MainWindow::editAlarmSettings()
{
    AlarmSettingsDialog dlg(&m_alarms, this);
    dlg.exec();
}

void MainWindow::editPollInterval()
{
    bool ok = false;
    const int ms = QInputDialog::getInt(this, tr("Intervalle d'acquisition"),
                                        tr("Pause entre deux cycles de lecture (ms) :\n"
                                           "Un cycle lit tous les capteurs supportés ; sa durée "
                                           "dépend du véhicule et de l'adaptateur."),
                                        m_engine.pollInterval(), 100, 10000, 100, &ok);
    if (!ok)
        return;
    m_engine.setPollInterval(ms);
    QSettings().setValue(QStringLiteral("acquisition/interval"), ms);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("À propos de %1").arg(AppInfo::name()),
                       tr("<h3>%1 %2</h3>"
                          "<p>Outil de diagnostic OBD-II générique pour adaptateurs "
                          "OBDLink SX (STN11xx) et compatibles ELM327.</p>"
                          "<ul><li>Lecture temps réel des capteurs (mode 01)</li>"
                          "<li>Lecture / effacement des codes défauts génériques (modes 03, 04, 07)</li>"
                          "<li>Lecture du VIN (mode 09)</li>"
                          "<li>Historique SQLite, trajets, statistiques, alarmes</li>"
                          "<li>Exports CSV, PDF%3</li></ul>"
                          "<p>Données : <code>%4</code></p>"
                          "<p>Ce logiciel n'effectue aucune programmation, ni télécodage, "
                          "ni accès sécurisé aux calculateurs.</p>")
                           .arg(AppInfo::name(), AppInfo::version(),
                                ExcelExporter::isAvailable() ? tr(", Excel") : QString(),
                                AppInfo::dataDir().toHtmlEscaped()));
}

void MainWindow::updateActions()
{
    const bool ready = m_engine.isReady();
    const bool polling = m_engine.isPolling();

    m_acquireAction->setEnabled(ready);
    m_acquireAction->setText(polling ? tr("Arrêter l'acquisition") : tr("Démarrer l'acquisition"));
    m_vinAction->setEnabled(ready);
    m_dtcAction->setEnabled(ready);
    m_tripAction->setEnabled(ready || m_trips.isRunning());
    m_tripAction->setText(m_trips.isRunning() ? tr("Arrêter le trajet") : tr("Démarrer un trajet"));
    m_tripsWidget->setCanStart(ready);
}

void MainWindow::refreshStatistics()
{
    if (m_statistics->source() == StatisticsWidget::Source::LiveSession)
        m_statistics->setSamples(m_liveSamples, tr("Session en cours – %1 mesures").arg(m_liveSamples.size()));
    else
        m_statistics->setSamples(m_history->currentSamples(), m_history->currentSessionLabel());
}

void MainWindow::status(const QString& message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_trips.isRunning())
        stopTrip();

    m_engine.disconnectAdapter();
    m_db.endSession(m_sessionId);
    m_sessionId = -1;

    QSettings cfg;
    cfg.setValue(QStringLiteral("window/geometry"), saveGeometry());
    cfg.setValue(QStringLiteral("window/state"), saveState());

    event->accept();
}
