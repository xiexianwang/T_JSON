// ============================================================
// test_jsonframeparser.cpp - TJsonProtocolParser 单元测试
// 覆盖：JSON 状态帧解析 / ACK 状态码 / 抓拍帧（校验和/帧尾/坐标）
// ============================================================

#include <QtTest>
#include "infrastructure/tjsonprotocolparser.h"
#include "infrastructure/tjsonframe.h"

class TestJsonFrameParser : public QObject
{
    Q_OBJECT
private slots:
    void parseJson_validObject();
    void parseJson_invalid();
    void parseJson_nonObject();

    void parseAck_normal();
    void parseAck_incomplete();
    void parseAck_error();
    void parseAck_illegalPayload();

    void parseImageSnap_valid();
    void parseImageSnap_checksumMismatch();
    void parseImageSnap_missingFooter();
    void parseImageSnap_shortFrame();
};

void TestJsonFrameParser::parseJson_validObject()
{
    QJsonObject out;
    QByteArray payload = R"({"ControlType":"ZoomInfo","Pan":123.5,"Tilt":-45.2})";
    QVERIFY(TJsonProtocolParser::parseJson(payload, out));
    QCOMPARE(out.value("ControlType").toString(), QString("ZoomInfo"));
    QCOMPARE(out.value("Pan").toDouble(), 123.5);
    QCOMPARE(out.value("Tilt").toDouble(), -45.2);
}

void TestJsonFrameParser::parseJson_invalid()
{
    QJsonObject out;
    QByteArray payload = "{not valid json";
    QVERIFY(!TJsonProtocolParser::parseJson(payload, out));
}

void TestJsonFrameParser::parseJson_nonObject()
{
    QJsonObject out;
    QByteArray payload = "[1,2,3]"; // 数组不是对象
    QVERIFY(!TJsonProtocolParser::parseJson(payload, out));
}

void TestJsonFrameParser::parseAck_normal()
{
    // 载荷 2 字节大端：0x00 0x00 = 正常
    QByteArray payload = QByteArray::fromHex("0000");
    QCOMPARE(TJsonProtocolParser::parseAck(payload), quint8(0));
}

void TestJsonFrameParser::parseAck_incomplete()
{
    QByteArray payload = QByteArray::fromHex("0001");
    QCOMPARE(TJsonProtocolParser::parseAck(payload), quint8(1));
}

void TestJsonFrameParser::parseAck_error()
{
    QByteArray payload = QByteArray::fromHex("0002");
    QCOMPARE(TJsonProtocolParser::parseAck(payload), quint8(2));
}

void TestJsonFrameParser::parseAck_illegalPayload()
{
    // 载荷不足 2 字节 → 返回 0
    QByteArray payload = QByteArray::fromHex("00");
    QCOMPARE(TJsonProtocolParser::parseAck(payload), quint8(0));
}

// 构造完整抓拍帧并校验通过
static QByteArray buildSnapFrame(const QByteArray& jpeg, quint16 left, quint16 top,
                                 quint16 width, quint16 height, bool badChecksum, bool badFooter)
{
    QByteArray frame;
    frame.append(static_cast<char>(TJsonFrame::kSnapHeaderB1)); // EB
    frame.append(static_cast<char>(TJsonFrame::kSnapHeaderB2)); // 92
    frame.append(static_cast<char>(TJsonFrame::kSnapType));     // 04

    // jpegSize 4B 大端
    quint32 size = static_cast<quint32>(jpeg.size());
    frame.append(static_cast<char>((size >> 24) & 0xFF));
    frame.append(static_cast<char>((size >> 16) & 0xFF));
    frame.append(static_cast<char>((size >> 8) & 0xFF));
    frame.append(static_cast<char>(size & 0xFF));

    // 坐标 8B（left/top/width/height 各 2B 大端）
    auto appendU16 = [&frame](quint16 v) {
        frame.append(static_cast<char>((v >> 8) & 0xFF));
        frame.append(static_cast<char>(v & 0xFF));
    };
    appendU16(left);
    appendU16(top);
    appendU16(width);
    appendU16(height);

    frame.append(jpeg);

    // checksum：前 7 字节累加
    quint8 sum = 0;
    for (int i = 0; i < 7; ++i)
        sum += static_cast<quint8>(frame.at(i));
    frame.append(static_cast<char>(badChecksum ? (sum + 1) : sum));

    // 帧尾 FB 92
    frame.append(static_cast<char>(badFooter ? '\x00' : '\xFB'));
    frame.append('\x92');
    return frame;
}

void TestJsonFrameParser::parseImageSnap_valid()
{
    QByteArray jpeg = QByteArray::fromHex("FFD8FFE00010FFD9"); // 8 字节
    QByteArray frame = buildSnapFrame(jpeg, 100, 200, 640, 360, false, false);

    TJsonProtocolParser::SnapResult out;
    QVERIFY(TJsonProtocolParser::parseImageSnap(frame, out));
    QCOMPARE(out.jpegData, jpeg);
    QCOMPARE(out.location.left(), 100);
    QCOMPARE(out.location.top(), 200);
    QCOMPARE(out.location.width(), 640);
    QCOMPARE(out.location.height(), 360);
}

void TestJsonFrameParser::parseImageSnap_checksumMismatch()
{
    QByteArray jpeg = QByteArray::fromHex("FFD8FFE0");
    QByteArray frame = buildSnapFrame(jpeg, 1, 2, 3, 4, true, false);

    TJsonProtocolParser::SnapResult out;
    QVERIFY(!TJsonProtocolParser::parseImageSnap(frame, out));
}

void TestJsonFrameParser::parseImageSnap_missingFooter()
{
    QByteArray jpeg = QByteArray::fromHex("FFD8FFE0");
    QByteArray frame = buildSnapFrame(jpeg, 1, 2, 3, 4, false, true);

    TJsonProtocolParser::SnapResult out;
    QVERIFY(!TJsonProtocolParser::parseImageSnap(frame, out));
}

void TestJsonFrameParser::parseImageSnap_shortFrame()
{
    // 帧不足最小帧头 18 字节
    QByteArray frame = QByteArray::fromHex("EB9204");
    TJsonProtocolParser::SnapResult out;
    QVERIFY(!TJsonProtocolParser::parseImageSnap(frame, out));
}

QTEST_GUILESS_MAIN(TestJsonFrameParser)
#include "test_jsonframeparser.moc"