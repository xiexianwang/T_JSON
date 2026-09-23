// ============================================================
// test_aitargettype.cpp - AI 目标类型名称映射单元测试
// 覆盖：各算法模型下的 Class→中文名，未映射十六进制兜底
// ============================================================

#include <QtTest>
#include "core/AiTargetType.h"

class TestAiTargetType : public QObject
{
    Q_OBJECT
private slots:
    void airTarget();
    void personVehicleModel();
    void shipModel();
    void droneModel();
    void aircraftModel();
    void birdModel();
    void unmappedFallsBackToHex();
    void modelHighDigitIgnored();
};

void TestAiTargetType::airTarget()
{
    QCOMPARE(AiTargetType::name(2, 0xA0), QStringLiteral("空中目标"));
    QCOMPARE(AiTargetType::name(13, 0xA0), QStringLiteral("空中目标"));
}

void TestAiTargetType::personVehicleModel()
{
    QCOMPARE(AiTargetType::name(2, 0xA1), QStringLiteral("人"));
    QCOMPARE(AiTargetType::name(2, 0xA2), QStringLiteral("车"));
    QCOMPARE(AiTargetType::name(12, 0xA1), QStringLiteral("人"));
}

void TestAiTargetType::shipModel()
{
    QCOMPARE(AiTargetType::name(3, 0xA3), QStringLiteral("船"));
    QCOMPARE(AiTargetType::name(13, 0xA3), QStringLiteral("船"));
}

void TestAiTargetType::droneModel()
{
    QCOMPARE(AiTargetType::name(4, 0xA4), QStringLiteral("无人机"));
}

void TestAiTargetType::aircraftModel()
{
    QCOMPARE(AiTargetType::name(5, 0xA1), QStringLiteral("飞机"));
    QCOMPARE(AiTargetType::name(5, 0xA2), QStringLiteral("直升机"));
}

void TestAiTargetType::birdModel()
{
    QCOMPARE(AiTargetType::name(6, 0xA3), QStringLiteral("鸟"));
}

void TestAiTargetType::unmappedFallsBackToHex()
{
    QCOMPARE(AiTargetType::name(3, 0xA2), QStringLiteral("0xA2"));
    QCOMPARE(AiTargetType::name(0, 0xA1), QStringLiteral("0xA1"));
    QCOMPARE(AiTargetType::name(2, 0x00), QStringLiteral("0x00"));
}

void TestAiTargetType::modelHighDigitIgnored()
{
    QCOMPARE(AiTargetType::name(3, 0xA3), AiTargetType::name(13, 0xA3));
}

QTEST_GUILESS_MAIN(TestAiTargetType)
#include "test_aitargettype.moc"
