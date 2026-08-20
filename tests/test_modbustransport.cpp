// ============================================================
// test_modbustransport.cpp - ModbusTransport 单元测试
// 覆盖：CRC16 静态方法（MODBUS-RTU 标准多项式 0xA001）
// ============================================================

#include <QtTest>
#include "infrastructure/modbustransport.h"

class TestModbusTransport : public QObject
{
    Q_OBJECT
private slots:
    void crc16_knownVectors();
    void crc16_singleByte();
    void crc16_empty();
    void initialState();
};

// MODBUS-RTU CRC16 标准测试向量（多项式 0xA001，初始值 0xFFFF）
void TestModbusTransport::crc16_knownVectors()
{
    // 标准 MODBUS 查询帧示例：
    // 从站 01, 功能码 03, 起始 006B, 长度 0003
    QByteArray frame;
    frame.append(static_cast<char>(0x01)); // 从站地址
    frame.append(static_cast<char>(0x03)); // 功能码
    frame.append(static_cast<char>(0x00)); // 起始高字节
    frame.append(static_cast<char>(0x6B)); // 起始低字节
    frame.append(static_cast<char>(0x00)); // 长度高字节
    frame.append(static_cast<char>(0x03)); // 长度低字节
    quint16 crc = ModbusTransport::crc16(frame);
    QCOMPARE(crc, static_cast<quint16>(0x1774));
}

void TestModbusTransport::crc16_singleByte()
{
    // 单字节 0x00 的 CRC16
    QCOMPARE(ModbusTransport::crc16(QByteArray(1, 0x00)), static_cast<quint16>(0x40BF));
    // 单字节 0xFF 的 CRC16
    QCOMPARE(ModbusTransport::crc16(QByteArray(1, static_cast<char>(0xFF))), static_cast<quint16>(0x00FF));
}

void TestModbusTransport::crc16_empty()
{
    // 空数据的 CRC16 应为初始值 0xFFFF
    QCOMPARE(ModbusTransport::crc16(QByteArray()), static_cast<quint16>(0xFFFF));
}

void TestModbusTransport::initialState()
{
    ModbusTransport transport;
    QCOMPARE(transport.isOpen(), false);
}

QTEST_GUILESS_MAIN(TestModbusTransport)
#include "test_modbustransport.moc"