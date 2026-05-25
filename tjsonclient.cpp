#include "tjsonclient.h"
#include <QDataStream>
#include <QtEndian>
#include <QDebug>
#include <QJsonParseError>

TJsonClient::TJsonClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_retryCount(0)
    , m_maxRetries(10)
    , m_currentDelay(2000)
    , m_autoReconnectEnabled(false)
{
    connect(m_socket, &QTcpSocket::connected, this, &TJsonClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TJsonClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TJsonClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &TJsonClient::onReadyRead);

    // Heartbeat timer (10 seconds)
    m_heartbeatTimer->setInterval(10000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &TJsonClient::sendHeartbeat);
    
    // Reconnect timer setup
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TJsonClient::attemptReconnect);
}

TJsonClient::~TJsonClient()
{
    m_autoReconnectEnabled = false;
    m_socket->disconnectFromHost();
}

bool TJsonClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TJsonClient::connectToDevice(const QString& ip, quint16 port)
{
    m_autoReconnectEnabled = true;
    m_retryCount = 0;
    m_currentDelay = 2000;
    m_lastIp = ip;
    m_lastPort = port;
    m_reconnectTimer->stop();

    if (isConnected()) {
        m_socket->disconnectFromHost();
    }
    m_socket->connectToHost(ip, port);
}

void TJsonClient::disconnectDevice()
{
    m_autoReconnectEnabled = false;
    m_reconnectTimer->stop();
    m_socket->disconnectFromHost();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void TJsonClient::handleReconnect()
{
    if (!m_autoReconnectEnabled) return;
    if (m_reconnectTimer->isActive()) return;
    if (m_socket->state() != QAbstractSocket::UnconnectedState) return;

    m_reconnectTimer->start(m_currentDelay);
    emit reconnecting(m_retryCount + 1, 0); 
    
    m_currentDelay = qMin(m_currentDelay * 2, 60000);
    m_retryCount++;
}

void TJsonClient::attemptReconnect()
{
    m_socket->abort();
    m_socket->connectToHost(m_lastIp, m_lastPort);
}

void TJsonClient::sendJsonCmd(const QJsonObject& cmd, FrameType type)
{
    if (!isConnected()) return;

    QJsonDocument doc(cmd);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    sendBinaryCmd(type, jsonData);
}

void TJsonClient::sendBinaryCmd(FrameType type, const QByteArray& payload)
{
    if (!isConnected()) return;

    quint32 length = payload.size();
    QByteArray frame;
    frame.append(static_cast<char>(0xEC));
    frame.append(static_cast<char>(0x91));
    frame.append(static_cast<char>(type));
    
    // Big Endian length
    quint32 lenBE = qToBigEndian(length);
    frame.append(reinterpret_cast<const char*>(&lenBE), 4);
    
    if (length > 0) {
        frame.append(payload);
    }
    m_socket->write(frame);
}

void TJsonClient::sendSerialCmd(const QString& serialType, const QByteArray& data)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SerialControl";
    cmd["SerialType"] = serialType;
    
    QJsonObject serialData;
    serialData["Lens"] = data.size();
    serialData["Data"] = QString::fromLatin1(data.toHex().toUpper());
    
    cmd["SerialData"] = serialData;
    
    sendJsonCmd(cmd, FrameType::Control);
}

void TJsonClient::sendHeartbeat()
{
    if (!isConnected()) return;
    
    QByteArray frame = QByteArray::fromHex("EC911100000000");
    m_socket->write(frame);
}

void TJsonClient::onSocketConnected()
{
    m_buffer.clear();
    m_retryCount = 0;
    m_currentDelay = 2000;
    m_reconnectTimer->stop();
    m_heartbeatTimer->start();
    emit deviceConnected();
    
    // Send immediate heartbeat on connection
    sendHeartbeat();
}

void TJsonClient::onSocketDisconnected()
{
    m_heartbeatTimer->stop();
    emit deviceDisconnected();
    handleReconnect();
}

void TJsonClient::onSocketError(QAbstractSocket::SocketError)
{
    emit errorOccurred(m_socket->errorString());
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        handleReconnect();
    }
}

void TJsonClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    processBuffer();
}

void TJsonClient::processBuffer()
{
    const quint32 MAX_JSON_LENGTH = 10 * 1024 * 1024; 
    const quint32 MAX_JPEG_LENGTH = 50 * 1024 * 1024;

    while (m_buffer.size() >= 7) {
        quint8 b1 = static_cast<quint8>(m_buffer.at(0));
        quint8 b2 = static_cast<quint8>(m_buffer.at(1));
        
        if (b1 == 0xEC && b2 == 0x91) {
            quint32 length;
            memcpy(&length, m_buffer.constData() + 3, 4);
            length = qFromBigEndian(length);
            
            if (length > MAX_JSON_LENGTH) {
                qWarning() << "Abnormal JSON length detected:" << length << ". Discarding header.";
                m_buffer.remove(0, 2); 
                continue;
            }
            
            if (static_cast<quint32>(m_buffer.size()) < 7 + length) {
                return; 
            }
            
            quint8 type = static_cast<quint8>(m_buffer.at(2));
            QByteArray payload = m_buffer.mid(7, length);
            m_buffer.remove(0, 7 + length);
            
            if (type != static_cast<quint8>(FrameType::Heartbeat) && !payload.isEmpty()) {
                parseJsonFrame(payload);
            }
            
        } else if (b1 == 0xEB && b2 == 0x92) {
            if (m_buffer.size() < 18) return;
            
            quint32 jpegSize;
            memcpy(&jpegSize, m_buffer.constData() + 3, 4);
            jpegSize = qFromBigEndian(jpegSize);
            
            if (jpegSize > MAX_JPEG_LENGTH) {
                qWarning() << "Abnormal JPEG length detected:" << jpegSize << ". Discarding header.";
                m_buffer.remove(0, 2);
                continue;
            }
            
            quint32 totalFrameSize = 18 + jpegSize; 
            if (static_cast<quint32>(m_buffer.size()) < totalFrameSize) {
                return; 
            }
            
            QByteArray frameData = m_buffer.left(totalFrameSize);
            m_buffer.remove(0, totalFrameSize);
            
            parseImageSnapFrame(frameData);
        } else {
            int nextEc = m_buffer.indexOf(QByteArray::fromHex("EC91"), 1);
            int nextEb = m_buffer.indexOf(QByteArray::fromHex("EB92"), 1);
            
            int nextHeader = -1;
            if (nextEc != -1 && nextEb != -1) nextHeader = qMin(nextEc, nextEb);
            else if (nextEc != -1) nextHeader = nextEc;
            else if (nextEb != -1) nextHeader = nextEb;
            
            if (nextHeader != -1) {
                m_buffer.remove(0, nextHeader);
            } else {
                m_buffer.clear();
            }
        }
    }
}

void TJsonClient::parseJsonFrame(const QByteArray& payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        emit jsonReceived(doc.object());
    } else {
        qDebug() << "Failed to parse JSON:" << err.errorString();
    }
}

void TJsonClient::parseImageSnapFrame(const QByteArray& payload)
{
    if (payload.size() < 18) return;
    
    quint16 left, top, width, height;
    memcpy(&left, payload.constData() + 7, 2);
    memcpy(&top, payload.constData() + 9, 2);
    memcpy(&width, payload.constData() + 11, 2);
    memcpy(&height, payload.constData() + 13, 2);
    
    left = qFromBigEndian(left);
    top = qFromBigEndian(top);
    width = qFromBigEndian(width);
    height = qFromBigEndian(height);
    
    quint32 jpegSize;
    memcpy(&jpegSize, payload.constData() + 3, 4);
    jpegSize = qFromBigEndian(jpegSize);
    
    QByteArray jpegData = payload.mid(15, jpegSize);
    QRect loc(left, top, width, height);
    
    emit imageSnapped(jpegData, loc);
}