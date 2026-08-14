// ============================================================
// 文件: stm32tcptransport.h
// 描述: STM32-TCP-V4.0 电机 TCP 传输层。封装 QTcpSocket 的连接/
//       断开/读写，负责 5A A5 02 帧头组包与命令序列号管理。
//       仅负责字节级收发与帧封装，不包含电机业务指令语义。
// ============================================================

#ifndef STM32TCPTRANSPORT_H
#define STM32TCPTRANSPORT_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

class QTcpSocket;

class Stm32TcpTransport : public QObject
{
    Q_OBJECT
public:
    explicit Stm32TcpTransport(QObject *parent = nullptr);

    void open(const QString& ip, quint16 port);   // 连接电机 TCP 服务器
    void close();                                 // 断开连接
    bool isOpen() const;                          // 是否已连接

    // 当前命令序列号（自增前取值，供 cmd_id 使用，保持与原行为一致）
    int seq() const { return m_seq; }

    // 发送一帧 STM32-TCP-V4.0 指令（内部 ++seq 写入帧头）
    bool send(const QJsonObject& json);

signals:
    void dataReceived(const QByteArray& data);    // 收到原始数据
    void frameSent(const QByteArray& pkt);        // 已发送完整帧（含帧头）
    void errorOccurred(const QString& msg);       // 连接失败/运行错误

private:
    QTcpSocket* m_socket = nullptr;
    int m_seq = 0;                                // 命令序列号
    QString m_ip;                                 // 目标 IP（重连使用）
    quint16 m_port = 0;                           // 目标端口（重连使用）
};

#endif // STM32TCPTRANSPORT_H
