#pragma once

#include "OBD/ObdEngine.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

// Barre de connexion : choix du port (détection automatique), débit,
// mode simulation, connexion / déconnexion et état de l'adaptateur.
class ConnectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionWidget(QWidget* parent = nullptr);

public slots:
    void refreshPorts();
    void setEngineState(ObdEngine::State state);
    void setAdapterInfo(const QString& identity, const QString& protocol);

signals:
    void connectRequested(const QString& port, qint32 baudrate, bool simulate);
    void disconnectRequested();

private:
    void onConnectClicked();
    void saveSettings() const;

    QComboBox* m_portList;
    QComboBox* m_baudList;
    QCheckBox* m_simulate;
    QPushButton* m_refreshButton;
    QPushButton* m_connectButton;
    QLabel* m_statusLed;
    QLabel* m_statusText;

    ObdEngine::State m_state = ObdEngine::State::Disconnected;
};
