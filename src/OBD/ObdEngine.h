#pragma once

#include "Core/Transport.h"
#include "DTC/DTC.h"
#include "Models/VehicleSample.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QTimer>

#include <memory>

// Moteur OBD-II : file d'attente des commandes, une seule commande en vol,
// délai d'expiration, initialisation de l'adaptateur et acquisition cyclique.
class ObdEngine : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Initializing, Ready };
    Q_ENUM(State)

    explicit ObdEngine(QObject* parent = nullptr);
    ~ObdEngine() override;

    bool connectSerial(const QString& port, qint32 baudrate = 115200);
    bool connectSimulator();
    void disconnectAdapter();

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }

    void startPolling();
    void stopPolling();
    bool isPolling() const { return m_polling; }
    void setPollInterval(int ms) { m_pollIntervalMs = qMax(100, ms); }
    int pollInterval() const { return m_pollIntervalMs; }

    void requestVin();
    void requestDtcs();
    void clearDtcs();
    void sendRaw(const QString& command);

    QString adapterIdentity() const { return m_identity; }
    QString protocolName() const { return m_protocol; }
    QSet<int> supportedPids() const { return m_supported; }

signals:
    void stateChanged(ObdEngine::State state);
    void adapterReady(const QString& identity, const QString& protocol);
    void vinReceived(const QString& vin);
    void dtcsReceived(const QList<DTC>& dtcs);
    void dtcsCleared(bool success);
    void sampleReady(const VehicleSample& sample);
    void rawResponse(const QString& command, const QString& response);
    void errorOccurred(const QString& message);
    void pollingChanged(bool polling);

private:
    enum class Kind {
        Init, Identify, StnIdentify, SupportedPids, ProtocolNumber, ProtocolName,
        PollPid, PollVoltage, Vin, StoredDtc, PendingDtc, ClearDtc, Raw
    };

    struct Request
    {
        QString command;
        Kind kind = Kind::Raw;
        int pid = 0;
        int timeoutMs = 2000;
    };

    bool attachTransport(std::unique_ptr<Transport> transport);
    void initializeAdapter();
    void enqueue(const QString& command, Kind kind, int pid = 0, int timeoutMs = 2000);
    void sendNext();
    void onResponse(const QString& data);
    void onTimeout();
    void handle(const Request& req, const QString& data);
    void schedulePoll();
    void enqueuePollCycle();
    void setState(State s);

    std::unique_ptr<Transport> m_transport;
    QQueue<Request> m_queue;
    Request m_current;
    bool m_busy = false;

    QTimer m_timeoutTimer;
    QTimer m_pollTimer;
    int m_pollIntervalMs = 500;
    bool m_polling = false;
    bool m_cycleQueued = false;
    int m_consecutiveTimeouts = 0;

    State m_state = State::Disconnected;
    QString m_identity;
    QString m_protocol;
    QSet<int> m_supported;
    QList<int> m_pollPids;

    VehicleSample m_currentSample;
    QList<DTC> m_dtcBuffer;
};
