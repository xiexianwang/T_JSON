// ============================================================
// 文件: modbustransport.cpp
// 描述: MODBUS-RTU 串口传输层实现。
// ============================================================

#include "modbustransport.h"
#include <QSerialPort>

ModbusTransport::ModbusTransport(QObject *parent)
    : QObject(parent)
{
}

bool ModbusTransport::open(const QString& portName)
{
    close();

    m_serial = new QSerialPort(portName, this);
    m_serial->setBaudRate(QSerialPort::Baud9600);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_serial->errorString());
        delete m_serial;
        m_serial = nullptr;
        return false;
    }

    connect(m_serial, &QSerialPort::readyRead, this, [this]() {
        emit dataReceived(m_serial->readAll());
    });
    connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err != QSerialPort::NoError)
            emit errorOccurred(m_serial->errorString());
    });
    return true;
}

void ModbusTransport::close()
{
    if (m_serial) {
        m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
    }
}

bool ModbusTransport::isOpen() const
{
    return m_serial && m_serial->isOpen();
}

void ModbusTransport::send(const QByteArray& pkt)
{
    if (m_serial && m_serial->isOpen())
        m_serial->write(pkt);
}

bool ModbusTransport::parseReadRegisterResponse(const QByteArray& resp, quint16* value)
{
    // 最短长度：从站 + 功能码 + 字节数 + 2 数据 + 2 CRC = 7
    if (resp.size() < 7) return false;
    if (static_cast<quint8>(resp.at(1)) != 0x03) return false;   // 功能码
    if (static_cast<quint8>(resp.at(2)) != 0x02) return false;   // 字节数

    const quint8 hi = static_cast<quint8>(resp.at(3));
    const quint8 lo = static_cast<quint8>(resp.at(4));
    if (value) *value = static_cast<quint16>((hi << 8) | lo);
    return true;
}

quint16 ModbusTransport::crc16(const QByteArray& data)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < data.size(); pos++) {
        crc ^= (uint8_t)data[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
