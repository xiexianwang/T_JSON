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

private:
    QTcpSocket* m_socket;               // TCP Socket 实例
    QTimer* m_heartbeatTimer;           // 心跳定时器（周期 10 秒）
    TJsonFrameCodec m_codec;            // 协议帧编解码器（粘包/半包/重同步）

    // 断线自动重连相关参数
    QTimer* m_reconnectTimer;           // 重连延迟定时器（单次触发）
    QString m_lastIp;                   // 上次连接的 IP 地址
    quint16 m_lastPort;                 // 上次连接的端口号
    int m_retryCount;                   // 当前已重连次数
    int m_maxRetries;                   // 最大重连尝试次数（默认 10）
    int m_currentDelay;                 // 当前重连延迟（指数退避，初始 2 秒）
    bool m_autoReconnectEnabled;        // 自动重连是否启用

    void dispatchFrame(TJsonFrameKind kind, FrameType type, const QByteArray& payload);  // 分发解析后的完整帧
    void handleReconnect();             // 触发自动重连流程（指数退避调度）
};

#endif // TJSONCLIENT_H
