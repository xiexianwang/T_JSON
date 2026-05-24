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

enum class FrameType : quint8 {
    Status = 0x01,
    Control = 0x03,
    ImageSnap = 0x04,
    QueryImageParams = 0x05,
    SetAreaDot = 0x06,
    SetDisplayMode = 0x07,
    SetAlgoModel = 0x08,
    SetCaptureState = 0x09,
    SetDigitalZoom = 0x0A,
    SetPosReset = 0x0B,
    QueryTofu7Params = 0x0C,
    SetTofu7Params = 0x0D,
    QueryTofu7Ignore = 0x0E,
    SetTofu7Ignore = 0x0F,
    Heartbeat = 0x11,
    Ack = 0x12,
    SetLocation = 0x20
};

class TJsonClient : public QObject
{
    Q_OBJECT
public:
    explicit TJsonClient(QObject *parent = nullptr);
    ~TJsonClient();

    bool isConnected() const;

public slots:
    void connectToDevice(const QString& ip, quint16 port);
    void disconnectDevice();
    
    // 发送带有JSON负载的帧
    void sendJsonCmd(const QJsonObject& cmd, FrameType type = FrameType::Control);
    
    // 发送无负载纯头部指令
    void sendBinaryCmd(FrameType type, const QByteArray& payload = QByteArray());
    
    // 发送串口透传指令
    void sendSerialCmd(const QString& serialType, const QByteArray& data);

signals:
    void deviceConnected();
    void deviceDisconnected();
    void errorOccurred(const QString& errorMsg);
    
    // Auto-reconnect signals
    void reconnecting(int attempt, int maxRetries);
    void reconnectFailed();
    
    // JSON Status Frame (0x01)
    void jsonReceived(const QJsonObject& doc);
    
    // Image Snap Frame (0xEB 0x92 0x04)
    void imageSnapped(const QByteArray& jpegData, const QRect& location);

private slots:
    void onReadyRead();
    void sendHeartbeat();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void attemptReconnect();

private:
    QTcpSocket* m_socket;
    QTimer* m_heartbeatTimer;
    QByteArray m_buffer;
    
    // Reconnection parameters
    QTimer* m_reconnectTimer;
    QString m_lastIp;
    quint16 m_lastPort;
    int m_retryCount;
    int m_maxRetries;
    int m_currentDelay;
    bool m_autoReconnectEnabled;

    void processBuffer();
    void parseJsonFrame(const QByteArray& payload);
    void parseImageSnapFrame(const QByteArray& payload);
    void handleReconnect();
};

#endif // TJSONCLIENT_H