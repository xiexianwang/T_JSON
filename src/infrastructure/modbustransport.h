// ============================================================
// 文件: modbustransport.h
// 描述: MODBUS-RTU 串口传输层。封装 QSerialPort 的打开/关闭/读写，
//       固定波特率 9600-8-N-1，并提供 CRC16 校验计算。
//       仅负责字节级收发，不包含电机业务指令语义。
// ============================================================

#ifndef MODBUSTRANSPORT_H
#define MODBUSTRANSPORT_H

#include <QObject>
#include <QByteArray>
#include <QString>

class QSerialPort;

class ModbusTransport : public QObject
{
    Q_OBJECT
public:
    explicit ModbusTransport(QObject *parent = nullptr);

    bool open(const QString& portName);   // 打开串口（9600-8-N-1），成功返回 true
    void close();                         // 关闭串口
    bool isOpen() const;                  // 串口是否已打开
    void send(const QByteArray& pkt);     // 发送一帧 MODBUS 数据

    // MODBUS-RTU CRC16 校验（多项式 0xA001）
    static quint16 crc16(const QByteArray& data);

signals:
    void dataReceived(const QByteArray& data);      // 收到原始数据
    void errorOccurred(const QString& msg);         // 打开失败/运行错误

private:
    QSerialPort* m_serial = nullptr;
};

#endif // MODBUSTRANSPORT_H
