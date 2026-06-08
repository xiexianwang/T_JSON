// ============================================================
// 文件: tjsonclient.cpp
// 描述: TJsonClient 类的实现。实现了基于 TCP 的 T-JSON 协议
//       客户端，包括帧封装/解析、心跳保活、断线自动重连（指数
//       退避）、JSON 指令发送以及图像帧接收等功能。
// ============================================================

#include "tjsonclient.h"
#include <QDataStream>
#include <QtEndian>
#include <QDebug>
#include <QJsonParseError>
#include <QNetworkProxy>

// 构造函数：初始化 Socket、心跳定时器和重连定时器
TJsonClient::TJsonClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))            // 创建 TCP Socket
    , m_heartbeatTimer(new QTimer(this))        // 创建心跳定时器
    , m_reconnectTimer(new QTimer(this))        // 创建重连定时器
    , m_retryCount(0)                           // 初始重连次数
    , m_maxRetries(10)                          // 最大重连尝试 10 次
    , m_currentDelay(2000)                      // 初始重连延迟 2 秒
    , m_autoReconnectEnabled(false)             // 默认不启用自动重连
{
    m_socket->setProxy(QNetworkProxy::NoProxy); // 禁用系统代理，直连设备

    // 连接 Socket 信号与对应处理槽
    connect(m_socket, &QTcpSocket::connected, this, &TJsonClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TJsonClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TJsonClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &TJsonClient::onReadyRead);

    // 心跳定时器：每 10 秒发送一次心跳帧
    m_heartbeatTimer->setInterval(10000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &TJsonClient::sendHeartbeat);

    // 重连定时器：单次触发，超时时执行一次重连尝试
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TJsonClient::attemptReconnect);
}

// 析构函数：禁用自动重连并断开 Socket
TJsonClient::~TJsonClient()
{
    m_autoReconnectEnabled = false;
    m_heartbeatTimer->stop();
    m_reconnectTimer->stop();
    m_socket->abort();
}

// 检查当前 TCP 连接是否处于已连接状态
bool TJsonClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

// 连接到指定 IP 和端口
// 重置重连计数器并更新重连参数以备断线后使用
void TJsonClient::connectToDevice(const QString& ip, quint16 port)
{
    m_autoReconnectEnabled = true;          // 启用自动重连
    m_retryCount = 0;                       // 重置重连次数
    m_currentDelay = 2000;                  // 重置延迟为初始值
    m_lastIp = ip;                          // 保存 IP 用于重连
    m_lastPort = port;                      // 保存端口用于重连
    m_reconnectTimer->stop();               // 停止待处理的重连

    // 如果已连接则先断开
    if (isConnected()) {
        m_socket->disconnectFromHost();
    }
    m_socket->connectToHost(ip, port);      // 发起连接
}

// 主动断开设备连接
// 禁用自动重连并清理 Socket
void TJsonClient::disconnectDevice()
{
    m_autoReconnectEnabled = false;         // 禁用自动重连
    m_reconnectTimer->stop();               // 停止重连定时器
    m_socket->disconnectFromHost();         // 优雅断开
    // 如果尚未断开，强制终止连接
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

// 触发自动重连流程（指数退避调度）
// 仅在满足以下条件时生效：自动重连启用、重连定时器未激活、Socket 已断开
void TJsonClient::handleReconnect()
{
    if (!m_autoReconnectEnabled) return;        // 未启用手动重连
    if (m_reconnectTimer->isActive()) return;    // 已有待处理重连
    if (m_socket->state() != QAbstractSocket::UnconnectedState) return;  // 尚未断开

    m_reconnectTimer->start(m_currentDelay);    // 启动延迟定时器
    emit reconnecting(m_retryCount + 1, 0);     // 发射重连通知信号

    // 指数退避：每次延迟翻倍，上限 60 秒
    m_currentDelay = qMin(m_currentDelay * 2, 60000);
    m_retryCount++;                             // 累加重连计数
}

// 执行一次重连尝试：先 abort 清空 Socket 状态，再重新连接
void TJsonClient::attemptReconnect()
{
    m_socket->abort();
    m_socket->connectToHost(m_lastIp, m_lastPort);
}

// 将 QJsonObject 序列化为 JSON 字符串，以指定帧类型发送
void TJsonClient::sendJsonCmd(const QJsonObject& cmd, FrameType type)
{
    if (!isConnected()) return;

    QJsonDocument doc(cmd);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);  // 紧凑格式
    sendBinaryCmd(type, jsonData);
}

// 发送二进制协议帧
// 帧格式: [0xEC][0x91][type(1B)][length(4B big-endian)][payload]
void TJsonClient::sendBinaryCmd(FrameType type, const QByteArray& payload)
{
    if (!isConnected()) return;

    quint32 length = payload.size();
    QByteArray frame;
    frame.append(static_cast<char>(0xEC));      // 帧头字节 1
    frame.append(static_cast<char>(0x91));      // 帧头字节 2
    frame.append(static_cast<char>(type));      // 帧类型

    // 载荷长度，以大端序写入 4 字节
    quint32 lenBE = qToBigEndian(length);
    frame.append(reinterpret_cast<const char*>(&lenBE), 4);

    if (length > 0) {
        frame.append(payload);                  // 附加载荷
    }
    m_socket->write(frame);                     // 写入 Socket 发送
}

// 构建串口透传 JSON 指令
// 将底层串口协议数据（如 Pelco-D、VISCA）封装为 JSON 格式发送
void TJsonClient::sendSerialCmd(const QString& serialType, const QByteArray& data)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SerialControl";       // 控制类型标识
    cmd["SerialType"] = serialType;             // 串口协议类型

    QJsonObject serialData;
    serialData["Lens"] = data.size();           // 数据长度
    serialData["Data"] = QString::fromLatin1(data.toHex().toUpper());  // 数据转大写 HEX 字符串

    cmd["SerialData"] = serialData;

    sendJsonCmd(cmd, FrameType::Control);       // 以控制帧类型发送
}

// 发送心跳帧（固定字节：EC 91 11 00 00 00 00）
void TJsonClient::sendHeartbeat()
{
    if (!isConnected()) return;

    QByteArray frame = QByteArray::fromHex("EC911100000000");
    m_socket->write(frame);
}

// Socket 连接成功建立后的处理
// 清空缓冲区、重置重连参数、启动心跳定时器并立即发送一次心跳
void TJsonClient::onSocketConnected()
{
    m_buffer.clear();                           // 清空残留在缓冲区中的数据
    m_retryCount = 0;                           // 重置重连计数
    m_currentDelay = 2000;                      // 重置退避延迟
    m_reconnectTimer->stop();                   // 停止待处理重连
    m_heartbeatTimer->start();                  // 启动心跳
    emit deviceConnected();                     // 通知连接已建立

    // 连接成功后立即发送一次心跳以确认双向通信正常
    sendHeartbeat();
}

// Socket 连接断开后的处理
// 停止心跳并触发自动重连
void TJsonClient::onSocketDisconnected()
{
    m_heartbeatTimer->stop();                   // 停止心跳
    emit deviceDisconnected();                  // 通知连接已断开
    handleReconnect();                          // 触发自动重连
}

// Socket 错误处理
// 自动重连启用时静默重试（不弹窗）；否则转发错误通知 UI
void TJsonClient::onSocketError(QAbstractSocket::SocketError)
{
    if (m_autoReconnectEnabled) {
        if (m_socket->state() == QAbstractSocket::UnconnectedState) {
            handleReconnect();
        }
    } else {
        emit errorOccurred(m_socket->errorString());
    }
}

// Socket 可读数据的入口
// 将新到达的数据追加到缓冲区后调用 processBuffer 进行解析
void TJsonClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    processBuffer();
}

// 协议缓冲区解析核心逻辑
// 循环检测缓冲区中的帧头并提取完整帧，支持两种帧格式：
//   1. 标准帧头 0xEC91 —— JSON/控制/状态帧
//   2. 特殊帧头 0xEB92 —— 图像抓拍帧
// 对于无法识别的数据，向后搜索下一个有效帧头进行对齐
void TJsonClient::processBuffer()
{
    const quint32 MAX_JSON_LENGTH = 10 * 1024 * 1024;   // JSON 帧最大 10 MB
    const quint32 MAX_JPEG_LENGTH = 50 * 1024 * 1024;   // JPEG 帧最大 50 MB

    // 最小帧头为 7 字节（2 字节帧头 + 1 字节类型 + 4 字节长度）
    while (m_buffer.size() >= 7) {
        quint8 b1 = static_cast<quint8>(m_buffer.at(0));
        quint8 b2 = static_cast<quint8>(m_buffer.at(1));

        if (b1 == 0xEC && b2 == 0x91) {
            // ----- 标准帧 (0xEC91) 处理 -----
            quint32 length;
            memcpy(&length, m_buffer.constData() + 3, 4);
            length = qFromBigEndian(length);            // 大端转主机字节序

            // 长度合法性检查，防止恶意或异常数据导致内存问题
            if (length > MAX_JSON_LENGTH) {
                qWarning() << "Abnormal JSON length detected:" << length << ". Discarding header.";
                m_buffer.remove(0, 2);                  // 丢弃无效帧头的前 2 字节
                continue;
            }

            // 检查缓冲区是否已收齐完整帧（7 字节头 + 载荷长度）
            if (static_cast<quint32>(m_buffer.size()) < 7 + length) {
                return;                                 // 数据不足，等待更多数据
            }

            quint8 type = static_cast<quint8>(m_buffer.at(2));
            QByteArray payload = m_buffer.mid(7, length);
            m_buffer.remove(0, 7 + length);             // 从缓冲区移除已处理帧

            // ACK 帧：提取第二个字节作为状态码发射
            if (type == static_cast<quint8>(FrameType::Ack) && payload.size() >= 2) {
                quint8 statusCode = static_cast<quint8>(payload.at(1));
                emit ackReceived(statusCode);
            } else if (type != static_cast<quint8>(FrameType::Heartbeat) && !payload.isEmpty()) {
                // 非心跳帧且有载荷时，尝试按 JSON 解析
                parseJsonFrame(payload);
            }

        } else if (b1 == 0xEB && b2 == 0x92) {
            // ----- 图像抓拍帧 (0xEB92) 处理 -----
            // 图像帧固定帧头 18 字节：2(帧头) + 1(类型) + 4(JPEG大小) + 11(坐标/保留)
            if (m_buffer.size() < 18) return;

            quint32 jpegSize;
            memcpy(&jpegSize, m_buffer.constData() + 3, 4);
            jpegSize = qFromBigEndian(jpegSize);

            // JPEG 大小合法性检查
            if (jpegSize > MAX_JPEG_LENGTH) {
                qWarning() << "Abnormal JPEG length detected:" << jpegSize << ". Discarding header.";
                m_buffer.remove(0, 2);
                continue;
            }

            quint32 totalFrameSize = 18 + jpegSize;     // 完整帧大小
            if (static_cast<quint32>(m_buffer.size()) < totalFrameSize) {
                return;                                 // 数据不足
            }

            QByteArray frameData = m_buffer.left(totalFrameSize);
            m_buffer.remove(0, totalFrameSize);

            parseImageSnapFrame(frameData);              // 解析图像帧

        } else {
            // ----- 未知数据：向后搜索下一个有效帧头 -----
            int nextEc = m_buffer.indexOf(QByteArray::fromHex("EC91"), 1);
            int nextEb = m_buffer.indexOf(QByteArray::fromHex("EB92"), 1);

            // 取两个帧头中较近的一个作为对齐位置
            int nextHeader = -1;
            if (nextEc != -1 && nextEb != -1) nextHeader = qMin(nextEc, nextEb);
            else if (nextEc != -1) nextHeader = nextEc;
            else if (nextEb != -1) nextHeader = nextEb;

            if (nextHeader != -1) {
                m_buffer.remove(0, nextHeader);         // 跳到下一个帧头位置
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
        emit jsonReceived(doc.object());                // 成功，发送 JSON 对象
    } else {
        qDebug() << "Failed to parse JSON:" << err.errorString();   // 解析失败日志
    }
}

// 解析图像抓拍帧
// 从完整的图像帧数据中提取 JPEG 数据块和图像在画面中的位置区域
// 帧结构: [0xEB][0x92][type][jpegSize(4B)][left(2B)][top(2B)][width(2B)][height(2B)][reserved(2B)][jpegData]
void TJsonClient::parseImageSnapFrame(const QByteArray& payload)
{
    if (payload.size() < 18) return;

    // 从偏移 7 处读取画面区域坐标（大端序）
    quint16 left, top, width, height;
    memcpy(&left, payload.constData() + 7, 2);
    memcpy(&top, payload.constData() + 9, 2);
    memcpy(&width, payload.constData() + 11, 2);
    memcpy(&height, payload.constData() + 13, 2);

    left = qFromBigEndian(left);
    top = qFromBigEndian(top);
    width = qFromBigEndian(width);
    height = qFromBigEndian(height);

    // JPEG 数据大小（偏移 3 处）
    quint32 jpegSize;
    memcpy(&jpegSize, payload.constData() + 3, 4);
    jpegSize = qFromBigEndian(jpegSize);

    // JPEG 数据从偏移 15 处开始
    QByteArray jpegData = payload.mid(15, jpegSize);
    QRect loc(left, top, width, height);                // 图像在原始画面中的位置

    emit imageSnapped(jpegData, loc);                   // 发射图像快照信号
}