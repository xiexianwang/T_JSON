// ============================================================
// test_devicestate.cpp - DeviceState 状态模型单元测试
// 覆盖：默认值 / 字段赋值 / AI 目标列表增删
// ============================================================

#include <QtTest>
#include "core/DeviceState.h"

class TestDeviceState : public QObject
{
    Q_OBJECT
private slots:
    void defaults();
    void assignZoomInfoFields();
    void assignImageSettingFields();
    void aiTargetList();
};

void TestDeviceState::defaults()
{
    DeviceState s;
    QCOMPARE(s.isConnected, false);
    QCOMPARE(s.currentPan, 0.0);
    QCOMPARE(s.currentTilt, 0.0);
    QCOMPARE(s.currentVisZoom, 1.0);
    QCOMPARE(s.currentIrZoom, 1.0);
    QCOMPARE(s.latitude, 0.0);
    QCOMPARE(s.longitude, 0.0);
    QCOMPARE(s.altitude, 0.0);
    QVERIFY(s.latitudeRaw.isEmpty());
    QVERIFY(s.longitudeRaw.isEmpty());
    QCOMPARE(s.workMode, 0);
    QCOMPARE(s.currentPipShow, 0);
    QCOMPARE(s.aiObjectCount, 0);
    QVERIFY(s.aiTargets.isEmpty());
    QCOMPARE(s.resX, 2688);
    QCOMPARE(s.resY, 1520);
}

void TestDeviceState::assignZoomInfoFields()
{
    DeviceState s;
    s.currentVisZoom = 12.5;
    s.currentIrZoom = 4.0;
    s.camShowMode = 2;
    s.currentPan = 123.4;
    s.currentTilt = -45.6;
    s.latitudeRaw = "31.2301N";
    s.longitudeRaw = "121.4737E";
    s.latitude = 31.2301;
    s.longitude = 121.4737;
    s.altitude = 88.5;
    s.laserRange = 1250.0;

    QCOMPARE(s.currentVisZoom, 12.5);
    QCOMPARE(s.currentIrZoom, 4.0);
    QCOMPARE(s.camShowMode, 2);
    QCOMPARE(s.currentPan, 123.4);
    QCOMPARE(s.currentTilt, -45.6);
    QCOMPARE(s.latitudeRaw, QString("31.2301N"));
    QCOMPARE(s.longitudeRaw, QString("121.4737E"));
    QCOMPARE(s.latitude, 31.2301);
    QCOMPARE(s.longitude, 121.4737);
    QCOMPARE(s.altitude, 88.5);
    QCOMPARE(s.laserRange, 1250.0);
}

void TestDeviceState::assignImageSettingFields()
{
    DeviceState s;
    s.imgSize = 3;
    s.bitrate = 8192;
    s.codec = 0;
    s.workMode = 2;
    s.currentPipShow = 16;
    s.model = 12;
    s.maxVisFL = "150mm";
    s.maxIRFL = "75mm";
    s.resX = 1920;
    s.resY = 1080;

    QCOMPARE(s.imgSize, 3);
    QCOMPARE(s.bitrate, 8192);
    QCOMPARE(s.codec, 0);
    QCOMPARE(s.workMode, 2);
    QCOMPARE(s.currentPipShow, 16);
    QCOMPARE(s.model, 12);
    QCOMPARE(s.maxVisFL, QString("150mm"));
    QCOMPARE(s.maxIRFL, QString("75mm"));
    QCOMPARE(s.resX, 1920);
    QCOMPARE(s.resY, 1080);
}

void TestDeviceState::aiTargetList()
{
    DeviceState s;
    AiTargetItem t1;
    t1.id = "t_001";
    t1.cls = 0xA1;
    t1.distance = 320.0;
    t1.hasPoints = true;
    t1.left = 100; t1.top = 200; t1.right = 300; t1.bottom = 400;
    s.aiTargets.append(t1);

    AiTargetItem t2;
    t2.id = "t_002";
    t2.cls = 0xA2;
    t2.state = 0xB2;
    s.aiTargets.append(t2);

    QCOMPARE(s.aiTargets.size(), 2);
    QCOMPARE(s.aiTargets.at(0).id, QString("t_001"));
    QCOMPARE(s.aiTargets.at(0).cls, 0xA1);
    QCOMPARE(s.aiTargets.at(0).distance, 320.0);
    QVERIFY(s.aiTargets.at(0).hasPoints);
    QCOMPARE(s.aiTargets.at(0).right, 300);
    QCOMPARE(s.aiTargets.at(0).bottom, 400);
    QCOMPARE(s.aiTargets.at(1).cls, 0xA2);
    QCOMPARE(s.aiTargets.at(1).state, 0xB2);

    // 清空（AI 超时清理路径）
    s.aiTargets.clear();
    QCOMPARE(s.aiTargets.size(), 0);
}

QTEST_GUILESS_MAIN(TestDeviceState)
#include "test_devicestate.moc"