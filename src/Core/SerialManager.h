#pragma once

#include "Transport.h"

#include <QByteArray>
#include <QSerialPort>

// Transport série pour OBDLink SX (puce STN11xx) et clones ELM327.
class SerialManager : public Transport
{
    Q_OBJECT

public:
    explicit SerialManager(QObject* parent = nullptr);
    ~SerialManager() override;

    void setPort(const QString& portName, qint32 baudrate = 115200);

    bool open() override;
    void close() override;
    bool isConnected() const override;
    void sendCommand(const QString& command) override;
    QString description() const override;

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_serial;
    QByteArray m_buffer;
    QString m_portName;
    qint32 m_baudrate = 115200;
    bool m_opening = false;
};
