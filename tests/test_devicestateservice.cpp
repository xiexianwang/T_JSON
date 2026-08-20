#include <QtTest>
#include "ui/main/DeviceStateService.h"

class TestDeviceStateService : public QObject
{
    Q_OBJECT

private slots:
    void keepsDevicesIsolated();
    void removesDeviceSnapshot();
    void restoresStateAndAi();
};

void TestDeviceStateService::keepsDevicesIsolated()
{
    DeviceStateService service;
    auto stateA = std::make_shared<DeviceState>();
    auto stateB = std::make_shared<DeviceState>();
    stateA->currentPan = 10;
    stateB->currentPan = 20;

    service.updateState("a", stateA);
    service.updateState("b", stateB);

    QCOMPARE(service.snapshot("a").statePtr->currentPan, 10.0);
    QCOMPARE(service.snapshot("b").statePtr->currentPan, 20.0);
}

void TestDeviceStateService::removesDeviceSnapshot()
{
    DeviceStateService service;
    service.updateAi("a", QJsonObject{{"ObjectCount", 1}});
    service.deviceRemoved("a");

    QVERIFY(!service.snapshot("a").exists);
}

void TestDeviceStateService::restoresStateAndAi()
{
    DeviceStateService service;
    auto state = std::make_shared<DeviceState>();
    state->latitude = 31.2;
    service.updateState("a", state);
    service.updateAi("a", QJsonObject{{"WorkMode", 2}, {"ObjectCount", 3}});

    const DeviceSnapshot snapshot = service.snapshot("a");
    QVERIFY(snapshot.exists);
    QVERIFY(snapshot.hasState);
    QVERIFY(snapshot.hasAi);
    QCOMPARE(snapshot.statePtr->latitude, 31.2);
    QCOMPARE(snapshot.aiInfo.value("ObjectCount").toInt(), 3);

    snapshot.statePtr->latitude = 99;
    QCOMPARE(service.snapshot("a").statePtr->latitude, 31.2);
}

QTEST_GUILESS_MAIN(TestDeviceStateService)
#include "test_devicestateservice.moc"
