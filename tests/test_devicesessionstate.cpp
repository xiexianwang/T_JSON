#include <QtTest>
#include "ui/main/DeviceSessionState.h"

class TestDeviceSessionState : public QObject
{
    Q_OBJECT

private slots:
    void selectsFirstDevice();
    void switchingKeepsGeneration();
    void removedGenerationIsRejectedAfterRecreate();
};

void TestDeviceSessionState::selectsFirstDevice()
{
    DeviceSessionState state;
    QVERIFY(state.ensure("a"));
    QCOMPARE(state.selectedDeviceId(), QString("a"));
    QCOMPARE(state.generation("a"), quint64(1));
}

void TestDeviceSessionState::switchingKeepsGeneration()
{
    DeviceSessionState state;
    state.ensure("a");
    state.ensure("b");
    QVERIFY(state.select("b"));
    QCOMPARE(state.selectedDeviceId(), QString("b"));
    QCOMPARE(state.generation("a"), quint64(1));
    QCOMPARE(state.generation("b"), quint64(1));
}

void TestDeviceSessionState::removedGenerationIsRejectedAfterRecreate()
{
    DeviceSessionState state;
    state.ensure("a");
    const quint64 oldGeneration = state.generation("a");
    QVERIFY(state.remove("a"));
    QVERIFY(!state.accepts("a", oldGeneration));
    QVERIFY(state.ensure("a"));
    QCOMPARE(state.generation("a"), quint64(3));
    QVERIFY(!state.accepts("a", oldGeneration));
    QVERIFY(state.accepts("a", state.generation("a")));
}

QTEST_GUILESS_MAIN(TestDeviceSessionState)
#include "test_devicesessionstate.moc"
