// ============================================================
// 文件: tjsonclient.h
// 描述: T-JSON 协议客户端。基于 TCP Socket 实现与后端设备的
//       通信，支持 JSON 指令、二进制指令、串口透传、心跳保活
//       以及断线自动重连。协议帧使用固定帧头 + 长度 + 载荷格式。
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

// 帧类型枚举：定义 T-JSON 协议中所有指令帧的类型字节
// 每个值对应帧头的第三个字节，用于区分不同功能
enum class FrameType : quint8 {
    Status = 0x01,              // 状态上报帧（设备 -> 客户端）
    Control = 0x03,             // 通用控制帧
    ImageSnap = 0x04,           // 图像抓拍帧（特殊帧头 0xEB 0x92）
    QueryImageParams = 0x05,    // 查询图像参数
    SetAreaDot = 0x06,          // 设置区域/点位
    SetDisplayMode = 0x07,      // 设置显示模式（画中画等）
    SetAlgoModel = 0x08,        // 设置算法模型
    SetCaptureState = 0x09,     // 设置抓拍上传状态
    SetDigitalZoom = 0x0A,      // 设置数字变焦
    SetPosReset = 0x0B,         // 设置位置归零
    QueryTofu7Params = 0x0C,    // 查询 Tofu7 参数
    SetTofu7Params = 0x0D,      // 设置 Tofu7 参数
    QueryTofu7Ignore = 0x0E,    // 查询 Tofu7 忽略区域
    SetTofu7Ignore = 0x0F,      // 设置 Tofu7 忽略区域
    Heartbeat = 0x11,           // 心跳帧（双向保活）
    Ack = 0x12,                 // 确认应答帧（含状态码）
    SetLocation = 0x20          // 设置 GPS 经纬度位置
};

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
    QByteArray m_buffer;                // 接收缓冲区，用于粘包处理

    // 断线自动重连相关参数
    QTimer* m_reconnectTimer;           // 重连延迟定时器（单次触发）
    QString m_lastIp;                   // 上次连接的 IP 地址
    quint16 m_lastPort;                 // 上次连接的端口号
    int m_retryCount;                   // 当前已重连次数
    int m_maxRetries;                   // 最大重连尝试次数（默认 10）
    int m_currentDelay;                 // 当前重连延迟（指数退避，初始 2 秒）
    bool m_autoReconnectEnabled;        // 自动重连是否启用

    void processBuffer();               // 从缓冲区解析并分发完整协议帧
    void parseJsonFrame(const QByteArray& payload);             // 解析 JSON 载荷
    void parseImageSnapFrame(const QByteArray& payload);        // 解析图像抓拍帧
    void handleReconnect();             // 触发自动重连流程（指数退避调度）
};

#endif // TJSONCLIENT_H