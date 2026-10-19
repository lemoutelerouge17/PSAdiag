#include "SerialManager.h"

#include "Logger.h"

SerialManager::SerialManager(QObject* parent)
    : Transport(parent)
{
    connect(&m_serial, &QSerialPort::readyRead, this, &SerialManager::onReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred, this, &SerialManager::onError);
}

SerialManager::~SerialManager()
{
    if (m_serial.isOpen())
        m_serial.close();
}

void SerialManager::setPort(const QString& portName, qint32 baudrate)
{
    m_portName = portName;
    m_baudrate = baudrate;
}

bool SerialManager::open()
{
    if (m_serial.isOpen())
        m_serial.close();

    m_serial.setPortName(m_portName);
    m_serial.setBaudRate(m_baudrate);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    m_opening = true;
    const bool opened = m_serial.open(QIODevice::ReadWrite);
    m_opening = false;

    if (!opened) {
        const QString msg = tr("Impossible d'ouvrir %1 : %2")
                                .arg(m_portName, m_serial.errorString());
        Logger::error(msg);
        emit errorOccurred(msg);
        return false;
    }

    // Certains adaptateurs USB ont besoin de DTR/RTS actifs.
    m_serial.setDataTerminalReady(true);
    m_serial.setRequestToSend(true);
    m_serial.clear();
    m_buffer.clear();

    Logger::info(tr("Port %1 ouvert à %2 bauds").arg(m_portName).arg(m_baudrate));
    emit connected();
    return true;
}

void SerialManager::close()
{
    if (!m_serial.isOpen())
        return;

    m_serial.close();
    m_buffer.clear();
    Logger::info(tr("Port %1 fermé").arg(m_portName));
    emit disconnected();
}

bool SerialManager::isConnected() const
{
    return m_serial.isOpen();
}

QString SerialManager::description() const
{
    return tr("%1 @ %2 bauds").arg(m_portName).arg(m_baudrate);
}

void SerialManager::sendCommand(const QString& command)
{
    if (!m_serial.isOpen())
        return;

    QByteArray cmd = command.toLatin1();
    cmd.append('\r');
    m_serial.write(cmd);
    Logger::tx(command);
}

void SerialManager::onReadyRead()
{
    m_buffer.append(m_serial.readAll());

    // Une réponse ELM/STN se termine toujours par le prompt '>'.
    int idx;
    while ((idx = m_buffer.indexOf('>')) >= 0) {
        const QByteArray chunk = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);

        QString data = QString::fromLatin1(chunk);
        data.remove(QChar('\0'));
        const QString trimmed = data.trimmed();

        Logger::rx(QString(trimmed).replace('\r', QStringLiteral(" | ")));
        emit responseReceived(trimmed);
    }

    // Garde-fou : un flux sans prompt ne doit pas faire grossir le tampon indéfiniment.
    if (m_buffer.size() > 64 * 1024)
        m_buffer.clear();
}

void SerialManager::onError(QSerialPort::SerialPortError error)
{
    // Les erreurs d'ouverture sont déjà signalées par open().
    if (error == QSerialPort::NoError || m_opening)
        return;

    const QString msg = tr("Erreur série : %1").arg(m_serial.errorString());
    Logger::error(msg);
    emit errorOccurred(msg);

    // Adaptateur débranché : on ferme proprement.
    if (error == QSerialPort::ResourceError || error == QSerialPort::PermissionError) {
        if (m_serial.isOpen())
            m_serial.close();
        emit disconnected();
    }
}
