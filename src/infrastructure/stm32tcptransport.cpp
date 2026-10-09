// ============================================================
// 文件: stm32tcptransport.cpp
// 描述: STM32-TCP-V4.0 电机 TCP 传输层实现。
// ============================================================

#include "stm32tcptransport.h"
#include <QTcpSocket>
#include <QNetworkProxy>
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
    m_socket->setProxy(QNetworkProxy::NoProxy);
    m_buffer.clear();
    connect(m_socket, &QTcpSocket::readyRead, this, &Stm32TcpTransport::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_socket->errorString());
    });
    m_socket->connectToHost(ip, port);
}

void Stm32TcpTransport::close()
{
    m_buffer.clear();
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

// 接收处理：按帧头 [5A A5][flag 1B][len 2B 小端][seq 1B][crc 2B] 切分缓冲，
// 剥离 8 字节帧头后仅 emit 其中的 JSON 负载。
// 直接对整帧做 JSON 解析会因帧头前缀而失败（此前电机电流/状态回包无法解析的根因）。
void Stm32TcpTransport::onReadyRead()
{
    if (!m_socket) return;
    m_buffer.append(m_socket->readAll());

    static constexpr int kHeaderSize = 8;
    static const QByteArray kMagic = QByteArray::fromHex("5AA5");

    for (;;) {
        if (m_buffer.size() < kHeaderSize) return;

        // 对齐到帧头（丢弃前导噪声）
        if (m_buffer.at(0) != kMagic.at(0) || m_buffer.at(1) != kMagic.at(1)) {
            const int idx = m_buffer.indexOf(kMagic, 1);
            if (idx < 0) { m_buffer.clear(); return; }
            m_buffer.remove(0, idx);
            continue;
        }

        const quint16 len = static_cast<quint8>(m_buffer.at(3))
                          | (static_cast<quint8>(m_buffer.at(4)) << 8);
        if (m_buffer.size() < kHeaderSize + len) return;   // 半包：等待后续数据

        const QByteArray payload = m_buffer.mid(kHeaderSize, len);
        m_buffer.remove(0, kHeaderSize + len);
        if (!payload.isEmpty())
            emit dataReceived(payload);
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
