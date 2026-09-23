// ============================================================
// test_modbustransport.cpp - ModbusTransport 单元测试
// 覆盖：CRC16 静态方法（MODBUS-RTU 标准多项式 0xA001）
//       读保持寄存器应答解析（电流寄存器 0x000D）
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
    void parseReadRegister_valid();
    void parseReadRegister_invalid();
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

void TestModbusTransport::parseReadRegister_valid()
{
    // 读 0x000D 应答：01 03 02 07 D0 xx xx → 2000 (0x07D0)
    QByteArray resp;
    resp.append(static_cast<char>(0x01));
    resp.append(static_cast<char>(0x03));
    resp.append(static_cast<char>(0x02));
    resp.append(static_cast<char>(0x07));
    resp.append(static_cast<char>(0xD0));
    resp.append(static_cast<char>(0x00));
    resp.append(static_cast<char>(0x00));
    quint16 value = 0;
    QVERIFY(ModbusTransport::parseReadRegisterResponse(resp, &value));
    QCOMPARE(value, static_cast<quint16>(2000));

    // 零值应答
    QByteArray zero;
    zero.append(char(0x01));
    zero.append(char(0x03));
    zero.append(char(0x02));
    zero.append(char(0x00));
    zero.append(char(0x00));
    zero.append(char(0x00));
    zero.append(char(0x00));
    QVERIFY(ModbusTransport::parseReadRegisterResponse(zero, &value));
    QCOMPARE(value, static_cast<quint16>(0));
}

void TestModbusTransport::parseReadRegister_invalid()
{
    quint16 value = 0;
    // 过短
    QVERIFY(!ModbusTransport::parseReadRegisterResponse(QByteArray("\x01\x03\x02\x07", 4), &value));
    // 功能码非 0x03（写回显 0x06）
    QByteArray writeEcho("\x01\x06\x00\x0D\x00\x01\x00\x00", 8);
    QVERIFY(!ModbusTransport::parseReadRegisterResponse(writeEcho, &value));
    // 字节数非 0x02
    QByteArray odd("\x01\x03\x04\x00\x00\x00\x00\x00\x00", 8);
    QVERIFY(!ModbusTransport::parseReadRegisterResponse(odd, &value));
}

QTEST_GUILESS_MAIN(TestModbusTransport)
#include "test_modbustransport.moc"