�// ============================================================
// 文件: tjsonclient.cpp
// 描述: TJsonClient 类的实现。实现了基于 TCP � T-JSON 协�
//       客户�，包�帧封�/解析、心跳保活�断线自动重连（指数
//       �避）、JSON 指令发�以及图像帧接收等功能�
// ============================================================

#include "tjsonclient.h"
#include "core/EventBus.h"
#include <QDataStream>
#include <QtEndian>
#include <QDebug>
#include <QJsonParseError>
#include <QNetworkProxy>

// 构�函数：初�化 Socket、心跳定时器和重连定时器
TJsonClient::TJsonClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))            // 创建 TCP Socket
    , m_heartbeatTimer(new QTimer(this))        // 创建心跳定时�
    , m_reconnectTimer(new QTimer(this))        // 创建重连定时�
    , m_retryCount(0)                           // 初�重连�数
    , m_maxRetries(10)                          // �大重连尝� 10 �
    , m_currentDelay(2000)                      // 初�重连延� 2 �
    , m_autoReconnectEnabled(false)             // 默�不�用自动重�
{
    m_socket->setProxy(QNetworkProxy::NoProxy); // 禁用系统代理，直连��

    // 连接 Socket 信号与�应处理�
    connect(m_socket, &QTcpSocket::connected, this, &TJsonClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TJsonClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TJsonClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &TJsonClient::onReadyRead);

    // 心跳定时�：每 10 秒发送一次心跳帧
    m_heartbeatTimer->setInterval(10000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &TJsonClient::sendHeartbeat);

    // 重连定时�：单次触发，超时时执行一次重连尝�
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TJsonClient::attemptReconnect);
}

// 析构函数：�用�动重连并�� Socket
TJsonClient::~TJsonClient()
{
    m_autoReconnectEnabled = false;
    m_heartbeatTimer->stop();
    m_reconnectTimer->stop();
    m_socket->abort();
}

// �查当� TCP 连接�否�于已连接状�
bool TJsonClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

// 连接到指� IP 和��
// 重置重连计数器并更新重连参数以�断线后使用
void TJsonClient::connectToDevice(const QString& ip, quint16 port)
{
    m_autoReconnectEnabled = true;          // �用自动重�
    m_retryCount = 0;                       // 重置重连次数
    m_currentDelay = 2000;                  // 重置延迟为初始�
    m_lastIp = ip;                          // 保存 IP 用于重连
    m_lastPort = port;                      // 保存�口用于重�
    m_reconnectTimer->stop();               // 停�待处理的重�

    // 如果已连接则先断�
    if (isConnected()) {
        m_socket->disconnectFromHost();
    }
    m_socket->connectToHost(ip, port);      // 发起连接
}

// 主动��设�连�
// 禁用�动重连并清理 Socket
void TJsonClient::disconnectDevice()
{
    m_autoReconnectEnabled = false;         // 禁用�动重�
    m_reconnectTimer->stop();               // 停�重连定时器
    m_socket->disconnectFromHost();         // 优雅��
    // 如果尚未��，强制终止连�
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

// 触发�动重连流程（指数�避调度）
// 仅在满足以下条件时生效：�动重连启用�重连定时器��活�Socket 已断�
void TJsonClient::handleReconnect()
{
    if (!m_autoReconnectEnabled) return;        // ��用手动重�
    if (m_reconnectTimer->isActive()) return;    // 已有待�理重连
    if (m_socket->state() != QAbstractSocket::UnconnectedState) return;  // 尚未��

    m_reconnectTimer->start(m_currentDelay);    // �动延迟定时器
    emit reconnecting(m_retryCount + 1, 0);     // 发射重连通知信号

    // 指数�避：每�延迟翻倍，上限 60 �
    m_currentDelay = qMin(m_currentDelay * 2, 60000);
    m_retryCount++;                             // �加重连�数
}

// 执�一次重连尝试：� abort 清空 Socket 状�，再重新连�
void TJsonClient::attemptReconnect()
{
    m_socket->abort();
    m_socket->connectToHost(m_lastIp, m_lastPort);
}

// � QJsonObject 序列化为 JSON 字�串，以指定帧类型发�
void TJsonClient::sendJsonCmd(const QJsonObject& cmd, FrameType type)
{
    if (!isConnected()) return;

    QJsonDocument doc(cmd);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);  // 紧凑格式
    sendBinaryCmd(type, jsonData);
}

// 发�二进制协��
// 帧格�: [0xEC][0x91][type(1B)][length(4B big-endian)][payload]
void TJsonClient::sendBinaryCmd(FrameType type, const QByteArray& payload)
{
    if (!isConnected()) return;

    quint32 length = payload.size();
    QByteArray frame;
    frame.append(static_cast<char>(0xEC));      // 帧头字节 1
    frame.append(static_cast<char>(0x91));      // 帧头字节 2
    frame.append(static_cast<char>(type));      // 帧类�

    // 载荷长度，以大�序写� 4 字节
    quint32 lenBE = qToBigEndian(length);
    frame.append(reinterpret_cast<const char*>(&lenBE), 4);

    if (length > 0) {
        frame.append(payload);                  // 附加载荷
    }
    m_socket->write(frame);                     // 写入 Socket 发�
}

// 构建串口透传 JSON 指令
// 将底层串口协�数据（� Pelco-D、VISCA）封装为 JSON 格式发�
void TJsonClient::sendSerialCmd(const QString& serialType, const QByteArray& data)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SerialControl";       // 控制类型标识
    cmd["SerialType"] = serialType;             // 串口协�类型

    QJsonObject serialData;
    serialData["Lens"] = data.size();           // 数据长度
    serialData["Data"] = QString::fromLatin1(data.toHex().toUpper());  // 数据�大写 HEX 字�串

    cmd["SerialData"] = serialData;

    sendJsonCmd(cmd, FrameType::Control);       // 以控制帧类型发�
}

// 发�心跳帧（固定字节：EC 91 11 00 00 00 00�
void TJsonClient::sendHeartbeat()
{
    if (!isConnected()) return;

    QByteArray frame = QByteArray::fromHex("EC911100000000");
    m_socket->write(frame);
}

// Socket 连接成功建立后的处理
// 清空缓冲区�重�重连参数、启动心跳定时器并立即发送一次心�
void TJsonClient::onSocketConnected()
{
    m_buffer.clear();                           // 清空残留在缓冲区�的数�
    m_retryCount = 0;                           // 重置重连计数
    m_currentDelay = 2000;                      // 重置�避延�
    m_reconnectTimer->stop();                   // 停�待处理重连
    m_heartbeatTimer->start();                  // �动心�
    emit deviceConnected();
    EventBus::instance()->postDeviceConnected("default_device");                     // 通知连接已建�

    // 连接成功后立即发送一次心跳以�认双向�信正常
    sendHeartbeat();
}

// Socket 连接��后的处理
// 停�心跳并触发�动重�
void TJsonClient::onSocketDisconnected()
{
    m_heartbeatTimer->stop();                   // 停�心�
    emit deviceDisconnected();
    EventBus::instance()->postDeviceDisconnected("default_device");                  // 通知连接已断�
    handleReconnect();                          // 触发�动重�
}

// Socket 错�处理
// �动重连启用时静默重试（不弹窗）；否则�发错�通知 UI
void TJsonClient::onSocketError(QAbstractSocket::SocketError)
{
    if (m_autoReconnectEnabled) {
        if (m_socket->state() == QAbstractSocket::UnconnectedState) {
            handleReconnect();
        }
    } else {
        emit errorOccurred(m_socket->errorString());
        EventBus::instance()->postDeviceError("default_device", m_socket->errorString());
    }
}

// Socket �读数�的入�
// 将新到达的数�追加到缓冲区后调� processBuffer 进�解�
void TJsonClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    processBuffer();
}

// 协�缓冲区解析核心�辑
// ���测缓冲区�的帧头并提取完整帧，�持两种帧格式�
//   1. 标准帧头 0xEC91 —� JSON/控制/状�帧
//   2. 特殊帧头 0xEB92 —� 图像抓拍�
// 对于无法识别的数�，向后搜�下一�有效帧头进��齐
void TJsonClient::processBuffer()
{
    const quint32 MAX_JSON_LENGTH = 10 * 1024 * 1024;   // JSON 帧最� 10 MB
    const quint32 MAX_JPEG_LENGTH = 50 * 1024 * 1024;   // JPEG 帧最� 50 MB

    // �小帧头为 7 字节�2 字节帧头 + 1 字节类型 + 4 字节长度�
    while (m_buffer.size() >= 7) {
        quint8 b1 = static_cast<quint8>(m_buffer.at(0));
        quint8 b2 = static_cast<quint8>(m_buffer.at(1));

        if (b1 == 0xEC && b2 == 0x91) {
            // ----- 标准� (0xEC91) 处理 -----
            quint32 length;
            memcpy(&length, m_buffer.constData() + 3, 4);
            length = qFromBigEndian(length);            // 大��主机字节�

            // 长度合法性�查，防�恶意或异常数据导致内存��
            if (length > MAX_JSON_LENGTH) {
                qWarning() << "Abnormal JSON length detected:" << length << ". Discarding header.";
                m_buffer.remove(0, 2);                  // 丢弃无效帧头的前 2 字节
                continue;
            }

            // �查缓冲区�否已收齐完整帧（7 字节� + 载荷长度�
            if (static_cast<quint32>(m_buffer.size()) < 7 + length) {
                return;                                 // 数据不足，等待更多数�
            }

            quint8 type = static_cast<quint8>(m_buffer.at(2));
            QByteArray payload = m_buffer.mid(7, length);
            m_buffer.remove(0, 7 + length);             // 从缓冲区移除已�理�

            // ACK 帧：状�码� 2 字节大�整数
            if (type == static_cast<quint8>(FrameType::Ack) && payload.size() == 2) {
                quint16 statusCode = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(payload.constData()));
                emit ackReceived(static_cast<quint8>(statusCode));
            } else if (type != static_cast<quint8>(FrameType::Heartbeat) && !payload.isEmpty()) {
                // 非心跳帧且有载荷时，尝试� JSON 解析
                parseJsonFrame(payload);
            }

        } else if (b1 == 0xEB && b2 == 0x92) {
            // ----- 图像抓拍� (0xEB92) 处理 -----
            // 图像帧固定帧� 18 字节�2(帧头) + 1(类型) + 4(JPEG大小) + 11(坐标/保留)
            if (m_buffer.size() < 18) return;

            quint32 jpegSize;
            memcpy(&jpegSize, m_buffer.constData() + 3, 4);
            jpegSize = qFromBigEndian(jpegSize);

            // JPEG 大小合法性��
            if (jpegSize > MAX_JPEG_LENGTH) {
                qWarning() << "Abnormal JPEG length detected:" << jpegSize << ". Discarding header.";
                m_buffer.remove(0, 2);
                continue;
            }

            quint32 totalFrameSize = 18 + jpegSize;     // 完整帧大�
            if (static_cast<quint32>(m_buffer.size()) < totalFrameSize) {
                return;                                 // 数据不足
            }

            QByteArray frameData = m_buffer.left(totalFrameSize);
            m_buffer.remove(0, totalFrameSize);

            parseImageSnapFrame(frameData);              // 解析图像�

        } else {
            // ----- �知数�：向后搜�下一�有效帧头 -----
            int nextEc = m_buffer.indexOf(QByteArray::fromHex("EC91"), 1);
            int nextEb = m_buffer.indexOf(QByteArray::fromHex("EB92"), 1);

            // 取两�帧头�较近的一�作为对齐位置
            int nextHeader = -1;
            if (nextEc != -1 && nextEb != -1) nextHeader = qMin(nextEc, nextEb);
            else if (nextEc != -1) nextHeader = nextEc;
            else if (nextEb != -1) nextHeader = nextEb;

            if (nextHeader != -1) {
                m_buffer.remove(0, nextHeader);         // 跳到下一�帧头位置
            } else {
                m_buffer.clear();                       // 无可识别帧头，清空缓冲区
            }
        }
    }
}

// 解析 JSON 载荷
// 尝试将载荷解析为 JSON 对象，成功则发射 jsonReceived 信号
void TJsonClient::parseJsonFrame(const QByteArray& payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        emit jsonReceived(doc.object());
        EventBus::instance()->postJsonReceived("default_device", doc.object());                // 成功，发� JSON 对象
    } else {
        qDebug() << "Failed to parse JSON:" << err.errorString();   // 解析失败日志
    }
}

// 解析图像抓拍�
// 从完整的图像帧数��提取 JPEG 数据块和图像在画��的位�区域
// 帧结�:
//   [0xEB][0x92][0x04][jpegSize(4B)][left(2B)][top(2B)][width(2B)][height(2B)]
//   [jpegData(NB)][checksum(1B)][0xFB][0x92]
// 帧校� = (0xEB + 0x92 + 0x04 + jpegSize � 4 字节) & 0xFF
void TJsonClient::parseImageSnapFrame(const QByteArray& payload)
{
    if (payload.size() < 18) return;

    // JPEG 数据大小（偏� 3 处）
    quint32 jpegSize;
    memcpy(&jpegSize, payload.constData() + 3, 4);
    jpegSize = qFromBigEndian(jpegSize);

    quint32 totalFrameSize = 18 + jpegSize;
    if (static_cast<quint32>(payload.size()) < totalFrameSize) return;

    // 校验帧校验和：前 7 字节��
    quint8 expectedSum = 0;
    for (int i = 0; i < 7; ++i)
        expectedSum += static_cast<quint8>(payload.at(i));
    quint8 actualSum = static_cast<quint8>(payload.at(15 + jpegSize));
    if (actualSum != expectedSum) {
        qWarning() << "Image snap checksum mismatch: expected" << expectedSum << "got" << actualSum;
        return;
    }

    // 校验帧尾标识 0xFB 0x92
    if (static_cast<quint8>(payload.at(16 + jpegSize)) != 0xFB ||
        static_cast<quint8>(payload.at(17 + jpegSize)) != 0x92) {
        qWarning() << "Image snap footer mismatch";
        return;
    }

    // 从偏� 7 处�取画面区域坐标（大�序）
    quint16 left, top, width, height;
    memcpy(&left, payload.constData() + 7, 2);
    memcpy(&top, payload.constData() + 9, 2);
    memcpy(&width, payload.constData() + 11, 2);
    memcpy(&height, payload.constData() + 13, 2);

    left = qFromBigEndian(left);
    top = qFromBigEndian(top);
    width = qFromBigEndian(width);
    height = qFromBigEndian(height);

    // JPEG 数据从偏� 15 处开�
    QByteArray jpegData = payload.mid(15, jpegSize);
    QRect loc(left, top, width, height);                // 图像在原始画��的位�

    emit imageSnapped(jpegData, loc);
    EventBus::instance()->postImageSnapped("default_device", jpegData, loc);                   // 发射图像�照信�
}