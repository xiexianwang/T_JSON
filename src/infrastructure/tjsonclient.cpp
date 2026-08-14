// ============================================================
// 文件: tjsonclient.cpp
// 描述: TJsonClient 类的实现。基于 TCP 的 T-JSON 协议客户端，
//       负责 Socket 生命周期、心跳保活、断线自动重连（指数退避）
//       与事件分发。帧编解码由 TJsonFrameCodec 完成，载荷解析
//       由 TJsonProtocolParser 完成。
// ============================================================

#include "tjsonclient.h"
#include "infrastructure/tjsonprotocolparser.h"
#include <QtEndian>
#include <QDebug>
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

    // 达到最大重连次数：停止自动重连并通知失败
    if (m_retryCount >= m_maxRetries) {
        m_autoReconnectEnabled = false;
        emit reconnectFailed();                 // 重连最终失败
        return;
    }

    m_reconnectTimer->start(m_currentDelay);    // 启动延迟定时器
    emit reconnecting(m_retryCount + 1, m_maxRetries);  // 发射重连通知信号

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

// 发送二进制协议帧（组包由 TJsonFrameCodec 完成）
void TJsonClient::sendBinaryCmd(FrameType type, const QByteArray& payload)
{
    if (!isConnected()) return;

    m_socket->write(TJsonFrameCodec::buildStandardFrame(type, payload));
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

// 发送心跳帧
void TJsonClient::sendHeartbeat()
{
    if (!isConnected()) return;

    m_socket->write(TJsonFrameCodec::buildHeartbeatFrame());
}

// Socket 连接成功建立后的处理
// 清空缓冲区、重置重连参数、启动心跳定时器并立即发送一次心跳
void TJsonClient::onSocketConnected()
{
    m_codec.clear();                            // 清空残留在缓冲区中的数据
    m_retryCount = 0;                           // 重置重连计数
    m_currentDelay = 2000;                      // 重置退避延迟
    m_reconnectTimer->stop();                   // 停止待处理重连
    m_heartbeatTimer->start();                  // 启动心跳
    emit deviceConnected();

    // 连接成功后立即发送一次心跳以确认双向通信正常
    sendHeartbeat();
}

// Socket 连接断开后的处理
// 停止心跳并触发自动重连
void TJsonClient::onSocketDisconnected()
{
    m_heartbeatTimer->stop();                   // 停止心跳
    emit deviceDisconnected();
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
// 将新到达的数据追加到编解码器后切帧并分发
void TJsonClient::onReadyRead()
{
    m_codec.feed(m_socket->readAll());

    TJsonFrameKind kind;
    FrameType type;
    QByteArray payload;
    while (m_codec.nextFrame(kind, type, payload)) {
        dispatchFrame(kind, type, payload);
    }
}

// 完整帧分发：按帧类别解析并发射对应信号
//   - ACK 帧：状态码 2 字节大端整数
//   - 心跳帧：忽略
//   - 抓拍帧：解析 JPEG 数据与画面区域
//   - 其余标准帧：按 JSON 解析
void TJsonClient::dispatchFrame(TJsonFrameKind kind, FrameType type, const QByteArray& payload)
{
    if (kind == TJsonFrameKind::Snap) {
        TJsonProtocolParser::SnapResult result;
        if (TJsonProtocolParser::parseImageSnap(payload, result)) {
            emit imageSnapped(result.jpegData, result.location);
        }
        return;
    }

    if (type == FrameType::Ack && payload.size() == 2) {
        emit ackReceived(TJsonProtocolParser::parseAck(payload));
        return;
    }

    // 非心跳帧且有载荷时，尝试按 JSON 解析
    if (type != FrameType::Heartbeat && !payload.isEmpty()) {
        QJsonObject doc;
        if (TJsonProtocolParser::parseJson(payload, doc)) {
            emit jsonReceived(doc);
        }
    }
}
