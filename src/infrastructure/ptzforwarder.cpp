#include "ptzforwarder.h"
#include <QDebug>
#include <QTimer>
#include <QNetworkProxy>

PtzForwarder::PtzForwarder(QObject *parent)
    : QObject(parent),
      m_ptzClient(new QTcpSocket(this)),
      m_mockServer(new QTcpServer(this))
{
    m_ptzClient->setProxy(QNetworkProxy::NoProxy); // 禁止使用代理
    connect(m_ptzClient, &QTcpSocket::readyRead, this, &PtzForwarder::onPtzReadyRead);
    connect(m_ptzClient, &QTcpSocket::disconnected, this, &PtzForwarder::onPtzDisconnected);
    connect(m_mockServer, &QTcpServer::newConnection, this, &PtzForwarder::onNewMockConnection);
}

PtzForwarder::~PtzForwarder()
{
    stop();
}

void PtzForwarder::start(const QString& ptzIp, quint16 ptzPort, quint16 mockServerPort)
{
    stop();

    m_ptzIp = ptzIp;
    m_ptzPort = ptzPort;

    qDebug() << "PtzForwarder starting. Connecting to PTZ:" << ptzIp << ":" << ptzPort;
    m_ptzClient->connectToHost(ptzIp, ptzPort);

    if (m_mockServer->listen(QHostAddress::Any, mockServerPort)) {
        qDebug() << "Mock Serial Server listening on port:" << mockServerPort;
    } else {
        qDebug() << "Failed to start Mock Serial Server on port:" << mockServerPort;
    }
}

void PtzForwarder::setOffsets(double panOffset, double tiltOffset)
{
    m_panOffset = panOffset;
    m_tiltOffset = tiltOffset;
}

void PtzForwarder::flushZeroPosition()
{
    // 构造 Pan 位置响应帧 (0x59)：角度 0°
    QByteArray panFrame(7, 0);
    panFrame[0] = static_cast<char>(0xFF);
    panFrame[1] = 0x00;   // 地址 1
    panFrame[2] = 0x00;   // Cmd1
    panFrame[3] = 0x59;   // Cmd2: Pan Position Response
    panFrame[4] = 0x00;   // Data1: 高位
    panFrame[5] = 0x00;   // Data2: 低位
    panFrame[6] = static_cast<char>(0x00 + 0x00 + 0x59 + 0x00 + 0x00); // 校验和

    // 构造 Tilt 位置响应帧 (0x5B)：角度 0°
    QByteArray tiltFrame(7, 0);
    tiltFrame[0] = static_cast<char>(0xFF);
    tiltFrame[1] = 0x00;
    tiltFrame[2] = 0x00;
    tiltFrame[3] = 0x5B;
    tiltFrame[4] = 0x00;
    tiltFrame[5] = 0x00;
    tiltFrame[6] = static_cast<char>(0x00 + 0x00 + 0x5B + 0x00 + 0x00);

    for (auto client : m_mockClients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(panFrame);
            client->write(tiltFrame);
        }
    }
}

void PtzForwarder::stop()
{
    m_ptzClient->disconnectFromHost();
    m_mockServer->close();

    while (!m_mockClients.isEmpty()) {
        QTcpSocket* client = m_mockClients.takeFirst();
        client->disconnectFromHost();
        client->deleteLater();
    }
    m_buffer.clear();
}

void PtzForwarder::onPtzDisconnected()
{
    qDebug() << "PTZ disconnected, retrying in 3 seconds...";
    QTimer::singleShot(3000, this, &PtzForwarder::reconnectPtz);
}

void PtzForwarder::reconnectPtz()
{
    if (m_ptzClient->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "Reconnecting to PTZ:" << m_ptzIp << ":" << m_ptzPort;
        m_ptzClient->connectToHost(m_ptzIp, m_ptzPort);
    }
}

void PtzForwarder::onPtzReadyRead()
{
    QByteArray data = m_ptzClient->readAll();

    m_buffer.append(data);

    // 解析 Pelco-D
    while (m_buffer.size() >= 7) {
        // 查找帧头 FF
        int startIdx = m_buffer.indexOf(static_cast<char>(0xFF));
        if (startIdx == -1) {
            m_buffer.clear();
            break;
        }

        if (startIdx > 0) {
            m_buffer.remove(0, startIdx); // 丢弃 FF 之前的数据
        }

        if (m_buffer.size() < 7) {
            break; // 数据不够一帧
        }

        // 取出一帧
        QByteArray frame = m_buffer.left(7);
        
        // 校验和
        quint8 sum = 0;
        for (int i = 1; i < 6; ++i) {
            sum += static_cast<quint8>(frame[i]);
        }
        if (sum != static_cast<quint8>(frame[6])) {
            m_buffer.remove(0, 1); // 校验失败，寻找下一个 FF
            continue;
        }

        m_buffer.remove(0, 7); // 移除已处理的帧
        parsePelcoD(frame);
    }
}


void PtzForwarder::parsePelcoD(const QByteArray& frame)
{
    if (frame.size() < 7) return;

    quint8 addr = static_cast<quint8>(frame[1]);
    quint8 cmd1 = static_cast<quint8>(frame[2]);
    quint8 cmd2 = static_cast<quint8>(frame[3]);
    quint8 data1 = static_cast<quint8>(frame[4]);
    quint8 data2 = static_cast<quint8>(frame[5]);

    QByteArray outFrame = frame;
    bool modified = false;

    // PelcoD Extended Responses
    // 0x59 = Pan Position Response
    // 0x5B = Tilt Position Response
    if (cmd2 == 0x59) {
        quint16 panVal = (data1 << 8) | data2;
        double panAngle = panVal / 100.0;
        
        // 减去偏移量
        panAngle -= m_panOffset;
        while (panAngle < 0) panAngle += 360.0;
        while (panAngle >= 360.0) panAngle -= 360.0;

        quint16 newPanVal = static_cast<quint16>(panAngle * 100.0 + 0.5); // 四舍五入
        outFrame[4] = static_cast<char>((newPanVal >> 8) & 0xFF);
        outFrame[5] = static_cast<char>(newPanVal & 0xFF);
        modified = true;
        
        emit ptzAnglesUpdated(panAngle, -999.0); // 仅更新 Pan
    } else if (cmd2 == 0x5B) {
        quint16 tiltVal = (data1 << 8) | data2;
        double tiltAngle = tiltVal / 100.0;
        
        // 某些设备可能处理超过180度的俯仰，转为负值
        if (tiltAngle > 180.0 && tiltAngle <= 360.0) {
            tiltAngle = tiltAngle - 360.0;
        }
        
        // 减去偏移量
        tiltAngle -= m_tiltOffset;
        while (tiltAngle < -180.0) tiltAngle += 360.0;
        while (tiltAngle > 180.0) tiltAngle -= 360.0;

        double posTilt = tiltAngle;
        if (posTilt < 0) posTilt += 360.0;
        
        quint16 newTiltVal = static_cast<quint16>(posTilt * 100.0 + 0.5); // 四舍五入
        outFrame[4] = static_cast<char>((newTiltVal >> 8) & 0xFF);
        outFrame[5] = static_cast<char>(newTiltVal & 0xFF);
        modified = true;

        emit ptzAnglesUpdated(-999.0, tiltAngle); // 仅更新 Tilt
    }

    if (modified) {
        // 重新计算校验和
        quint8 sum = 0;
        for (int i = 1; i < 6; ++i) {
            sum += static_cast<quint8>(outFrame[i]);
        }
        outFrame[6] = static_cast<char>(sum);
    }

    // 转发 FF 开头的数据（即整个 Pelco-D 帧）
    for (auto client : m_mockClients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(outFrame);
        }
    }
}


void PtzForwarder::onNewMockConnection()
{
    while (QTcpSocket* client = m_mockServer->nextPendingConnection()) {
        qDebug() << "New mock client connected:" << client->peerAddress().toString();
        connect(client, &QTcpSocket::disconnected, this, &PtzForwarder::onMockClientDisconnected);
        // 如果客户端也发送数据过来，我们可以转发给转台（双向透传）
        connect(client, &QTcpSocket::readyRead, this, [this, client]() {
            if (m_ptzClient->state() == QAbstractSocket::ConnectedState) {
                m_ptzClient->write(client->readAll());
            }
        });
        m_mockClients.append(client);
    }
}

void PtzForwarder::onMockClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        qDebug() << "Mock client disconnected:" << client->peerAddress().toString();
        m_mockClients.removeAll(client);
        client->deleteLater();
    }
}
