// ============================================================
// test_streamwatchdog.cpp - RTSP 重连/看门狗纯逻辑单元测试
// 覆盖：停帧判定（未武装/无时间戳/未超阈值/超阈值/边界）
//       退避抖动（范围/非负/零抖动/边界夹取）
// ============================================================

#include <QtTest>
#include "infrastructure/streamwatchdog.h"
#include "infrastructure/rtspbackoff.h"

class TestStreamWatchdog : public QObject
{
    Q_OBJECT
private slots:
    void notArmed_neverStalled();
    void armed_noTimestamp_notStalled();
    void armed_withinTimeout_notStalled();
    void armed_beyondTimeout_stalled();
    void boundary_equalTimeout_notStalled();

    void backoff_noJitter_isBase();
    void backoff_jitterWithinBounds();
    void backoff_neverNegative();
    void backoff_jitterBoundsCalc();
};

void TestStreamWatchdog::notArmed_neverStalled()
{
    QVERIFY(!rtspIsStalled(false, 1000, 1000000, 10000));
    QVERIFY(!rtspIsStalled(false, 0, 1000000, 10000));
}

void TestStreamWatchdog::armed_noTimestamp_notStalled()
{
    QVERIFY(!rtspIsStalled(true, 0, 1000000, 10000));
    QVERIFY(!rtspIsStalled(true, -5, 1000000, 10000));
}

void TestStreamWatchdog::armed_withinTimeout_notStalled()
{
    QVERIFY(!rtspIsStalled(true, 100000, 109000, 10000));
}

void TestStreamWatchdog::armed_beyondTimeout_stalled()
{
    QVERIFY(rtspIsStalled(true, 100000, 111000, 10000));
}

void TestStreamWatchdog::boundary_equalTimeout_notStalled()
{
    QVERIFY(!rtspIsStalled(true, 100000, 110000, 10000));
    QVERIFY(rtspIsStalled(true, 100000, 110001, 10000));
}

void TestStreamWatchdog::backoff_noJitter_isBase()
{
    const QPair<int, int> b = rtspJitterBounds(1000, 0);
    QCOMPARE(b.first, 1000);
    QCOMPARE(b.second, 1000);
    QCOMPARE(rtspApplyJitter(1000, 0, 0), 1000);
    QCOMPARE(rtspApplyJitter(1000, 0, 999), 1000);
}

void TestStreamWatchdog::backoff_jitterWithinBounds()
{
    const QPair<int, int> b = rtspJitterBounds(1000, 20);
    QCOMPARE(b.first, 800);
    QCOMPARE(b.second, 1200);
    for (int i = 0; i < 200; ++i) {
        const int v = rtspApplyJitter(1000, 20, static_cast<quint32>(i * 7919));
        QVERIFY(v >= 800 && v <= 1200);
    }
}

void TestStreamWatchdog::backoff_neverNegative()
{
    const QPair<int, int> b = rtspJitterBounds(3, 100);
    QCOMPARE(b.first, 0);
    for (int i = 0; i < 100; ++i) {
        const int v = rtspApplyJitter(3, 100, static_cast<quint32>(i));
        QVERIFY(v >= 0);
    }
}

void TestStreamWatchdog::backoff_jitterBoundsCalc()
{
    const QPair<int, int> b = rtspJitterBounds(1, 5);
    QVERIFY(b.first <= b.second);
    const QPair<int, int> b2 = rtspJitterBounds(1000, 500);
    QCOMPARE(b2.first, 0);
    QCOMPARE(b2.second, 2000);
}

QTEST_GUILESS_MAIN(TestStreamWatchdog)
#include "test_streamwatchdog.moc"
