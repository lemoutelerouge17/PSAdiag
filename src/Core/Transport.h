#pragma once

#include <QObject>
#include <QString>

// Interface commune entre l'adaptateur réel (port série OBDLink SX / ELM327)
// et le simulateur intégré. Une réponse complète se termine par le prompt '>'.
class Transport : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~Transport() override = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;
    virtual void sendCommand(const QString& command) = 0;
    virtual QString description() const = 0;

signals:
    // Réponse complète de l'adaptateur (sans le prompt '>').
    void responseReceived(const QString& data);
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
};
