// ============================================================
// 文件: stm32tcptransport.cpp
// 描述: STM32-TCP-V4.0 电机 TCP 传输层实现。
// ============================================================

#include "stm32tcptransport.h"
#include <QTcpSocket>
#include <QJsonDocument>

Stm32TcpTransport::Stm32TcpTransport(QObject *parent)
    : QObject(parent)
{
}

void Stm32TcpTransport::open(const QString& ip, quint16 port)
{
    close();
    m_ip = ip;
    m_port = port;

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        emit dataReceived(m_socket->readAll());
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_socket->errorString());
    });
    m_socket->connectToHost(ip, port);
}

void Stm32TcpTransport::close()
{
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool Stm32TcpTransport::isOpen() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

bool Stm32TcpTransport::send(const QJsonObject& json)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        // 未连接时尝试重新连接（保持原行为：短超时同步等待）
        if (m_socket) {
            m_socket->connectToHost(m_ip, m_port);
            m_socket->waitForConnected(500);
        }
        if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
            emit errorOccurred(QObject::tr("电机 TCP 未连接"));
            return false;
        }
    }

    QByteArray payload = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray header;
    header.resize(8);
    // Magic: 0xA55A (小端序 -> 5A A5)
    header[0] = static_cast<char>(0x5A);
    header[1] = static_cast<char>(0xA5);
    header[2] = static_cast<char>(0x02); // Cmd: 0x02
    quint16 len = payload.size();
    header[3] = static_cast<char>(len & 0xFF);
    header[4] = static_cast<char>((len >> 8) & 0xFF);
    header[5] = static_cast<char>(++m_seq & 0xFF);
    header[6] = 0x00; // CRC
    header[7] = 0x00; // CRC

    QByteArray pkt = header + payload;
    m_socket->write(pkt);
    emit frameSent(pkt);
    return true;
}
