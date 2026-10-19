#pragma once

#include "Transport.h"

#include <QElapsedTimer>
#include <QStringList>

// Simulateur d'adaptateur ELM327/STN11xx sur bus CAN 11 bits.
// Permet de tester toute l'application sans véhicule ni adaptateur.
class SimulatorTransport : public Transport
{
    Q_OBJECT

public:
    explicit SimulatorTransport(QObject* parent = nullptr);

    bool open() override;
    void close() override;
    bool isConnected() const override;
    void sendCommand(const QString& command) override;
    QString description() const override;

private:
    QString handle(const QString& cmd);
    QString handleMode01(int pid);
    QString hex(const QList<int>& bytes) const;
    QString vinResponse() const;
    QString dtcResponse(int mode) const;

    bool m_open = false;
    bool m_echo = true;
    bool m_spaces = true;
    bool m_headers = false;
    QStringList m_storedDtcs;
    QElapsedTimer m_clock;
};
