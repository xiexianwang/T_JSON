// ============================================================
// 文件: tjsonclient.h
// 描述: T-JSON 协议客户端。基于 TCP Socket 实现与后端设备的
//       通信，支持 JSON 指令、二进制指令、串口透传、心跳保活
//       以及断线自动重连。协议帧的编解码与载荷解析已拆分为
//       TJsonFrameCodec / TJsonProtocolParser，本类仅负责
//       Socket 传输、连接生命周期与事件分发。
// ============================================================

#ifndef TJSONCLIENT_H
#define TJSONCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QRect>
#include <QImage>
#include "infrastructure/tjsonframe.h"
#include "infrastructure/tjsonframecodec.h"

// T-JSON 协议客户端类
// 封装了与设备建立 TCP 连接、发送/接收协议帧、心跳保活、
// 断线自动重连等完整生命周期管理
class TJsonClient : public QObject
{
    Q_OBJECT
public:
    explicit TJsonClient(QObject *parent = nullptr);
    ~TJsonClient();

    bool isConnected() const;   // 返回当前 TCP 连接状态

public slots:
    void connectToDevice(const QString& ip, quint16 port);   // 连接到指定 IP:Port
    void disconnectDevice();                                  // 主动断开连接

    // 发送带有 JSON 负载的帧：将 QJsonObject 序列化后作为载荷发送
    void sendJsonCmd(const QJsonObject& cmd, FrameType type = FrameType::Control);

    // 发送无负载或纯二进制载荷的帧（如查询指令）
    void sendBinaryCmd(FrameType type, const QByteArray& payload = QByteArray());

    // 发送串口透传指令：通过 JSON 控制帧封装底层串口协议数据
    void sendSerialCmd(const QString& serialType, const QByteArray& data);

signals:
    void deviceConnected();                     // 成功建立连接时发射
    void deviceDisconnected();                  // 连接断开时发射
    void errorOccurred(const QString& errorMsg); // 发生 Socket 错误时发射

    // 自动重连相关信号
    void reconnecting(int attempt, int maxRetries);  // 正在尝试第 attempt 次重连
    void reconnectFailed();                          // 重连最终失败

    // 收到 JSON 状态帧 (FrameType::Status) 时发射
    void jsonReceived(const QJsonObject& doc);

    // 收到 ACK 应答帧 (FrameType::Ack) 时发射，携带状态码
    void ackReceived(quint8 statusCode);

    // 收到图像抓拍帧 (特殊帧头 0xEB 0x92) 时发射，包含 JPEG 数据和位置信息
    void imageSnapped(const QByteArray& jpegData, const QRect& location);

private slots:
    void onReadyRead();                 // Socket 有数据可读时的处理入口
    void sendHeartbeat();               // 定时发送心跳帧
    void onSocketConnected();           // Socket 连接建立后的回调
    void onSocketDisconnected();        // Socket 断开后的回调
    void onSocketError(QAbstractSocket::SocketError socketError);  // Socket 错误处理
    void attemptReconnect();            // 执行一次重连尝试
    void checkActivityTimeout();        // 心跳应答超时检测（半开连接主动探测）
    void onConnectTimeout();            // 单次连接尝试超时（收敛不可达主机的系统 TCP 超时）

private:
    // 心跳应答超时（毫秒）：连续该时长未收到任何上行数据即判定连接假死。
    // TCP 半开（如拔设备端网线）时系统栈可能数十秒才报错，本机制把恢复时间
    // 收敛为「检测时长 + 退避初始延迟」，与拔线位置无关。
    // 心跳周期 10s（文档约定），阈值取 15s：容忍一次心跳丢失，又不会误判。
    static constexpr int kHeartbeatIntervalMs = 10000;   // 心跳周期（10s）
    static constexpr int kActivityCheckMs      = 1000;   // 探测周期（1s）
    static constexpr int kActivityTimeoutMs    = 15000;  // 连续无上行数据阈值（1.5 个心跳）

    // 重连退避：初始 1s、上限 5s，与 RTSP 侧量级一致，避免拔线后等待数分钟。
    static constexpr int kReconnectInitialDelayMs = 1000;
    static constexpr int kReconnectMaxDelayMs     = 5000;
    // 单次连接尝试超时：不可达主机时系统 TCP 可能 21s 才报错，此处主动收敛。
    static constexpr int kConnectTimeoutMs        = 4000;

    QTcpSocket* m_socket;               // TCP Socket 实例
    QTimer* m_heartbeatTimer;           // 心跳定时器（周期 10 秒）
    TJsonFrameCodec m_codec;            // 协议帧编解码器（粘包/半包/重同步）

    // 断线自动重连相关参数
    QTimer* m_reconnectTimer;           // 重连延迟定时器（单次触发）
    QString m_lastIp;                   // 上次连接的 IP 地址
    quint16 m_lastPort;                 // 上次连接的端口号
    int m_retryCount;                   // 当前已重连次数
    int m_maxRetries;                   // 最大重连尝试次数（0 = 无限重试）
    int m_currentDelay;                 // 当前重连延迟（指数退避，初始 1 秒）
    bool m_autoReconnectEnabled;        // 自动重连是否启用

    QTimer* m_activityTimer;            // 活动看门狗定时器（检测半开连接）
    QTimer* m_connectTimer;             // 单次连接尝试超时定时器
    qint64 m_lastRxMs = 0;              // 最近一次收到任意上行数据的时刻（ms）

    void dispatchFrame(TJsonFrameKind kind, FrameType type, const QByteArray& payload);  // 分发解析后的完整帧
    void handleReconnect();             // 触发自动重连流程（指数退避调度）
    void forceReconnectNow();           // 应用层主动判定假死并立即重连
};

#endif // TJSONCLIENT_H
