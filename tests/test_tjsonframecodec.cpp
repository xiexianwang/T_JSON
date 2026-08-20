// ============================================================
// test_tjsonframecodec.cpp - TJsonFrameCodec 单元测试
// 覆盖：标准帧组包 / 心跳帧 / 单帧切分 / 粘包 / 半包 /
//       非法长度 / 未知数据重同步 / 抓拍帧切分
// ============================================================

#include <QtTest>
#include "infrastructure/tjsonframecodec.h"
#include "infrastructure/tjsonframe.h"

class TestTJsonFrameCodec : public QObject
{
    Q_OBJECT
private slots:
    // ---- 发送组包 ----
    void buildStandardFrame_basic();
    void buildStandardFrame_emptyPayload();
    void buildHeartbeatFrame_fixedBytes();

    // ---- 接收切分 ----
    void nextFrame_singleFrame();
    void nextFrame_stickyTwoFrames();
    void nextFrame_halfPacket_thenComplete();
    void nextFrame_halfPacket_headerOnly();
    void nextFrame_abnormalLength_resync();
    void nextFrame_unknownData_resync();
    void nextFrame_snapFrame();
};

void TestTJsonFrameCodec::buildStandardFrame_basic()
{
    QByteArray payload = QByteArray::fromHex("7B2261223A317D"); // {"a":1}
    QByteArray frame = TJsonFrameCodec::buildStandardFrame(FrameType::Control, payload);

    QCOMPARE(frame.size(), 7 + payload.size());
    QCOMPARE(static_cast<quint8>(frame.at(0)), TJsonFrame::kHeaderB1);   // 0xEC
    QCOMPARE(static_cast<quint8>(frame.at(1)), TJsonFrame::kHeaderB2);   // 0x91
    QCOMPARE(static_cast<quint8>(frame.at(2)), static_cast<quint8>(FrameType::Control));
    // 长度 4 字节大端
    QCOMPARE(static_cast<quint8>(frame.at(3)), quint8(0));
    QCOMPARE(static_cast<quint8>(frame.at(4)), quint8(0));
    QCOMPARE(static_cast<quint8>(frame.at(5)), quint8(0));
    QCOMPARE(static_cast<quint8>(frame.at(6)), quint8(payload.size()));
    QCOMPARE(frame.mid(7), payload);
}

void TestTJsonFrameCodec::buildStandardFrame_emptyPayload()
{
    QByteArray frame = TJsonFrameCodec::buildStandardFrame(FrameType::Heartbeat, QByteArray());
    QCOMPARE(frame.size(), 7);
    QCOMPARE(static_cast<quint8>(frame.at(6)), quint8(0)); // 长度 0
}

void TestTJsonFrameCodec::buildHeartbeatFrame_fixedBytes()
{
    QByteArray frame = TJsonFrameCodec::buildHeartbeatFrame();
    QCOMPARE(frame.toHex().toUpper(), QByteArray("EC911100000000"));
}

void TestTJsonFrameCodec::nextFrame_singleFrame()
{
    QByteArray payload = "hello";
    QByteArray frame = TJsonFrameCodec::buildStandardFrame(FrameType::Status, payload);

    TJsonFrameCodec codec;
    codec.feed(frame);

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(kind, TJsonFrameKind::Standard);
    QCOMPARE(type, FrameType::Status);
    QCOMPARE(out, payload);
    QCOMPARE(codec.bufferedBytes(), 0);
}

void TestTJsonFrameCodec::nextFrame_stickyTwoFrames()
{
    QByteArray payload1 = "aaa";
    QByteArray payload2 = "bbbb";
    QByteArray data = TJsonFrameCodec::buildStandardFrame(FrameType::Control, payload1)
                    + TJsonFrameCodec::buildStandardFrame(FrameType::SetAlgoModel, payload2);

    TJsonFrameCodec codec;
    codec.feed(data);

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(type, FrameType::Control);
    QCOMPARE(out, payload1);
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(type, FrameType::SetAlgoModel);
    QCOMPARE(out, payload2);
    QCOMPARE(codec.bufferedBytes(), 0);
}

void TestTJsonFrameCodec::nextFrame_halfPacket_thenComplete()
{
    QByteArray payload = "0123456789";
    QByteArray frame = TJsonFrameCodec::buildStandardFrame(FrameType::Status, payload);

    TJsonFrameCodec codec;
    // 分两次喂入：先头 7 字节 + 3 字节载荷，再补剩余
    codec.feed(frame.left(10));

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(!codec.nextFrame(kind, type, out)); // 半包，无帧产出
    QCOMPARE(codec.bufferedBytes(), 10);

    codec.feed(frame.mid(10));
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(out, payload);
}

void TestTJsonFrameCodec::nextFrame_halfPacket_headerOnly()
{
    TJsonFrameCodec codec;
    codec.feed(QByteArray::fromHex("EC9101")); // 仅 3 字节，不足最小帧头

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(!codec.nextFrame(kind, type, out));
    QCOMPARE(codec.bufferedBytes(), 3);
}

void TestTJsonFrameCodec::nextFrame_abnormalLength_resync()
{
    // 非法超长长度（> 10MB）应丢弃帧头并继续对齐
    QByteArray badFrame;
    badFrame.append(static_cast<char>(TJsonFrame::kHeaderB1));
    badFrame.append(static_cast<char>(TJsonFrame::kHeaderB2));
    badFrame.append(static_cast<char>(FrameType::Control));
    badFrame.append('\x7F');   // 长度 0x7F000000 远超限制
    badFrame.append('\x00');
    badFrame.append('\x00');
    badFrame.append('\x00');
    badFrame.append("tail-garbage");

    // 后接一个合法帧
    QByteArray goodPayload = "ok";
    QByteArray goodFrame = TJsonFrameCodec::buildStandardFrame(FrameType::Status, goodPayload);

    TJsonFrameCodec codec;
    codec.feed(badFrame + goodFrame);

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(type, FrameType::Status);
    QCOMPARE(out, goodPayload);
}

void TestTJsonFrameCodec::nextFrame_unknownData_resync()
{
    // 垃圾前缀后跟合法帧，应重同步并切出合法帧
    QByteArray junk = QByteArray::fromHex("DEADBEEF0011");
    QByteArray goodPayload = "x";
    QByteArray goodFrame = TJsonFrameCodec::buildStandardFrame(FrameType::Ack, goodPayload);

    TJsonFrameCodec codec;
    codec.feed(junk + goodFrame);

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(type, FrameType::Ack);
    QCOMPARE(out, goodPayload);
}

void TestTJsonFrameCodec::nextFrame_snapFrame()
{
    // 构造抓拍帧：[EB][92][04][jpegSize 4B][coord 8B][jpegData][checksum][FB][92]
    QByteArray jpeg = QByteArray::fromHex("FFD8FFE0FFF0"); // 伪 JPEG 数据 6 字节
    QByteArray frame;
    frame.append(static_cast<char>(TJsonFrame::kSnapHeaderB1)); // EB
    frame.append(static_cast<char>(TJsonFrame::kSnapHeaderB2)); // 92
    frame.append(static_cast<char>(TJsonFrame::kSnapType));     // 04
    // jpegSize 4B 大端
    frame.append(static_cast<char>(0));
    frame.append(static_cast<char>(0));
    frame.append(static_cast<char>(0));
    frame.append(static_cast<char>(jpeg.size()));
    // 坐标 left/top/width/height 各 2B（不校验，仅结构）
    frame.append(QByteArray(8, '\x00'));
    frame.append(jpeg);
    frame.append('\x00'); // checksum（Parser 才会校验）
    frame.append('\xFB');
    frame.append('\x92');

    TJsonFrameCodec codec;
    codec.feed(frame);

    TJsonFrameKind kind;
    FrameType type;
    QByteArray out;
    QVERIFY(codec.nextFrame(kind, type, out));
    QCOMPARE(kind, TJsonFrameKind::Snap);
    QCOMPARE(type, FrameType::ImageSnap);
    QCOMPARE(out, frame); // 抓拍帧整体作为 payload 返回（含帧头）
    QCOMPARE(codec.bufferedBytes(), 0);
}

QTEST_GUILESS_MAIN(TestTJsonFrameCodec)
#include "test_tjsonframecodec.moc"