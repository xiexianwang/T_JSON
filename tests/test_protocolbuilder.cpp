// ============================================================
// test_protocolbuilder.cpp - Pelco-D / VISCA 协议组包单元测试
// 覆盖：Pelco-D 7 字节帧结构与 checksum、PtzDir 位组合、
//       云台运动/停止/绝对角度、预置位三连、红外镜头、
//       雨刷、VISCA 变倍/变焦/停止
// ============================================================

#include <QtTest>
#include "infrastructure/pelcodprotocol.h"
#include "infrastructure/viscaprotocol.h"

class TestProtocolBuilder : public QObject
{
    Q_OBJECT
private slots:
    void pelcod_basicFrame();
    void pelcod_moveDirs();
    void pelcod_stop();
    void pelcod_panTo();
    void pelcod_tiltTo();
    void pelcod_setZero();
    void pelcod_presets();
    void pelcod_irLens();
    void pelcod_lensStop();
    void pelcod_wiper();
    void visca_zoom();
    void visca_focus();
    void visca_stop();
};

// 校验 Pelco-D 帧结构：FF addr cmd1 cmd2 data1 data2 checksum
static void verifyPelcoFrame(const QByteArray& pkt, quint8 addr, quint8 cmd1, quint8 cmd2,
                             quint8 data1, quint8 data2)
{
    QCOMPARE(pkt.size(), 7);
    QCOMPARE(static_cast<quint8>(pkt.at(0)), quint8(0xFF));
    QCOMPARE(static_cast<quint8>(pkt.at(1)), addr);
    QCOMPARE(static_cast<quint8>(pkt.at(2)), cmd1);
    QCOMPARE(static_cast<quint8>(pkt.at(3)), cmd2);
    QCOMPARE(static_cast<quint8>(pkt.at(4)), data1);
    QCOMPARE(static_cast<quint8>(pkt.at(5)), data2);
    quint8 expected = static_cast<quint8>((addr + cmd1 + cmd2 + data1 + data2) % 256);
    QCOMPARE(static_cast<quint8>(pkt.at(6)), expected);
}

void TestProtocolBuilder::pelcod_basicFrame()
{
    // build() 原始组包：checksum = 前 5 字节和 % 256
    QByteArray pkt = PelcoDProtocol::build(0x01, 0x00, 0x08, 0x20, 0x30);
    quint8 expected = static_cast<quint8>((0x01 + 0x00 + 0x08 + 0x20 + 0x30) % 256);
    QCOMPARE(static_cast<quint8>(pkt.at(6)), expected);
}

void TestProtocolBuilder::pelcod_moveDirs()
{
    // 单方向
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::Up, 0x20, 0x30),
                     0x01, 0x00, 0x08, 0x20, 0x30);
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::Down, 0x20, 0x30),
                     0x01, 0x00, 0x10, 0x20, 0x30);
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::Left, 0x20, 0x30),
                     0x01, 0x00, 0x04, 0x20, 0x30);
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::Right, 0x20, 0x30),
                     0x01, 0x00, 0x02, 0x20, 0x30);

    // 对角方向 = 位组合
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::UpLeft, 0x20, 0x30),
                     0x01, 0x00, 0x0C, 0x20, 0x30);
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::UpRight, 0x20, 0x30),
                     0x01, 0x00, 0x0A, 0x20, 0x30);
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::DownLeft, 0x20, 0x30),
                     0x01, 0x00, 0x14, 0x20, 0x30);
    verifyPelcoFrame(PelcoDProtocol::buildMove(0x01, PtzDir::DownRight, 0x20, 0x30),
                     0x01, 0x00, 0x12, 0x20, 0x30);
}

void TestProtocolBuilder::pelcod_stop()
{
    verifyPelcoFrame(PelcoDProtocol::buildStop(0x01), 0x01, 0x00, 0x00, 0x00, 0x00);
}

void TestProtocolBuilder::pelcod_panTo()
{
    // panVal = 角度 * 100，高位在前
    verifyPelcoFrame(PelcoDProtocol::buildPanTo(0x01, 12345), 0x01, 0x00, 0x4B, 0x30, 0x39);
}

void TestProtocolBuilder::pelcod_tiltTo()
{
    // 业务层对负角做偏移后调用：tilt=-3° → tiltVal = 36000 + (-300) = 35700 (0x8B74)
    // 高位在前
    verifyPelcoFrame(PelcoDProtocol::buildTiltTo(0x01, 35700), 0x01, 0x00, 0x4D, 0x8B, 0x74);
}

void TestProtocolBuilder::pelcod_setZero()
{
    verifyPelcoFrame(PelcoDProtocol::buildSetZero(0x01), 0x01, 0x00, 0x49, 0x00, 0x00);
}

void TestProtocolBuilder::pelcod_presets()
{
    verifyPelcoFrame(PelcoDProtocol::buildSetPreset(0x01, 3), 0x01, 0x00, 0x03, 0x03, 0x00);
    verifyPelcoFrame(PelcoDProtocol::buildCallPreset(0x01, 3), 0x01, 0x00, 0x07, 0x03, 0x00);
    verifyPelcoFrame(PelcoDProtocol::buildClearPreset(0x01, 3), 0x01, 0x00, 0x05, 0x03, 0x00);
}

void TestProtocolBuilder::pelcod_irLens()
{
    // 红外镜头变倍放大/缩小（speed 在 data2）
    verifyPelcoFrame(PelcoDProtocol::buildZoomIn(0x01, 0x20), 0x01, 0x00, 0x20, 0x00, 0x20);
    verifyPelcoFrame(PelcoDProtocol::buildZoomOut(0x01, 0x20), 0x01, 0x00, 0x40, 0x00, 0x20);
    // 红外变焦
    verifyPelcoFrame(PelcoDProtocol::buildFocusIn(0x01), 0x01, 0x01, 0x00, 0x00, 0x00);
    verifyPelcoFrame(PelcoDProtocol::buildFocusOut(0x01), 0x01, 0x00, 0x80, 0x00, 0x00);
}

void TestProtocolBuilder::pelcod_lensStop()
{
    // zoom=true 停止变倍：0x00 0x60；false 停止变焦：0x01 0x80
    verifyPelcoFrame(PelcoDProtocol::buildLensStop(0x01, true), 0x01, 0x00, 0x60, 0x00, 0x00);
    verifyPelcoFrame(PelcoDProtocol::buildLensStop(0x01, false), 0x01, 0x01, 0x80, 0x00, 0x00);
}

void TestProtocolBuilder::pelcod_wiper()
{
    verifyPelcoFrame(PelcoDProtocol::buildWiperOn(0x01), 0x01, 0x00, 0x09, 0x00, 0x01);
    verifyPelcoFrame(PelcoDProtocol::buildWiperOff(0x01), 0x01, 0x00, 0x0B, 0x00, 0x01);
}

void TestProtocolBuilder::visca_zoom()
{
    // 变倍 Tele 速度 5：81 01 04 07 25 FF
    QByteArray tele = ViscaProtocol::buildZoom(1, true, 5);
    QCOMPARE(tele.toHex().toUpper(), QByteArray("8101040725FF"));

    // 变倍 Wide 速度 3：81 01 04 07 33 FF
    QByteArray wide = ViscaProtocol::buildZoom(1, false, 3);
    QCOMPARE(wide.toHex().toUpper(), QByteArray("8101040733FF"));
}

void TestProtocolBuilder::visca_focus()
{
    // 变焦 Far：81 01 04 08 02 FF；Near：81 01 04 08 03 FF
    QByteArray far = ViscaProtocol::buildFocus(1, true);
    QCOMPARE(far.toHex().toUpper(), QByteArray("8101040802FF"));
    QByteArray near = ViscaProtocol::buildFocus(1, false);
    QCOMPARE(near.toHex().toUpper(), QByteArray("8101040803FF"));
}

void TestProtocolBuilder::visca_stop()
{
    // 变倍停止：81 01 04 07 00 FF；变焦停止：81 01 04 08 00 FF
    QByteArray zoomStop = ViscaProtocol::buildStop(1, true);
    QCOMPARE(zoomStop.toHex().toUpper(), QByteArray("8101040700FF"));
    QByteArray focusStop = ViscaProtocol::buildStop(1, false);
    QCOMPARE(focusStop.toHex().toUpper(), QByteArray("8101040800FF"));
}

QTEST_GUILESS_MAIN(TestProtocolBuilder)
#include "test_protocolbuilder.moc"