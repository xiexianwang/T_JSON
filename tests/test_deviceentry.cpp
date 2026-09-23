#include <QtTest/QtTest>
#include <QJsonObject>
#include <QJsonDocument>

#include "core/DeviceEntry.h"

// ============================================================================
// DeviceEntry 序列化 / 校验 单元测试
// ============================================================================
class TestDeviceEntry : public QObject
{
    Q_OBJECT

private slots:
    // ---- 稳定 id 生成 ----
    void newIdIsUniqueAndPrefixed()
    {
        const QString a = DeviceEntry::newId(QStringLiteral("dev_"));
        const QString b = DeviceEntry::newId(QStringLiteral("dev_"));
        QVERIFY(a.startsWith(QStringLiteral("dev_")));
        QVERIFY(b.startsWith(QStringLiteral("dev_")));
        QVERIFY(a != b);
        QVERIFY(!a.contains(QLatin1Char('-')));
    }

    // ---- 显式类型：设备与分组不再靠 RTSP 推断 ----
    void deviceWithoutRtspStillIsDevice()
    {
        DeviceEntry e;
        e.id = QStringLiteral("dev_1");
        e.type = DeviceNodeType::Device;
        e.name = QStringLiteral("无RTSP设备");
        e.ip = QStringLiteral("10.0.0.9");
        e.rtspUrl.clear(); // 关键：RTSP 为空仍是设备

        const DeviceEntry back = DeviceEntry::fromJson(e.toJson());
        QCOMPARE(back.type, DeviceNodeType::Device);
        QVERIFY(back.isDevice());
        QCOMPARE(back.name, QStringLiteral("无RTSP设备"));
    }

    // ---- 名称含空格不被截断 ----
    void nameWithSpacesSurvives()
    {
        DeviceEntry e = DeviceEntry::makeDevice(QStringLiteral("东门 摄像头"), QStringLiteral("192.168.1.10"));
        QCOMPARE(e.name, QStringLiteral("东门 摄像头"));
        const DeviceEntry back = DeviceEntry::fromJson(e.toJson());
        QCOMPARE(back.name, QStringLiteral("东门 摄像头"));
    }

    // ---- 往返一致性：稳定 id / 配置 / 连接参数 ----
    void roundTripPreservesFields()
    {
        DeviceEntry e = DeviceEntry::makeDevice(QStringLiteral("A"), QStringLiteral("192.168.1.20"), 8089);
        e.config.motorTcpIp = QStringLiteral("192.168.1.55");
        e.config.zoomSpeed = 7;
        e.config.targetRefMap[0x0201] = 4.5;

        const DeviceEntry back = DeviceEntry::fromJson(e.toJson());
        QCOMPARE(back.id, e.id);
        QCOMPARE(back.type, DeviceNodeType::Device);
        QCOMPARE(back.ip, QStringLiteral("192.168.1.20"));
        QCOMPARE(back.config.motorTcpIp, QStringLiteral("192.168.1.55"));
        QCOMPARE(back.config.zoomSpeed, static_cast<quint8>(7));
        QCOMPARE(back.config.targetRefMap.value(0x0201), 4.5);
    }

    // ---- 旧格式兼容：无 type / 无 id 时应推断并补齐 ----
    void legacyJsonInference()
    {
        QJsonObject legacy{
            {QStringLiteral("name"), QStringLiteral("旧设备")},
            {QStringLiteral("ip"), QStringLiteral("192.168.1.30")},
            {QStringLiteral("rtspUrl"), QStringLiteral("rtsp://192.168.1.30/live")},
            {QStringLiteral("deviceConfig"), QJsonObject{{QStringLiteral("zoomSpeed"), 3}}}
        };
        const DeviceEntry e = DeviceEntry::fromJson(legacy);
        QCOMPARE(e.type, DeviceNodeType::Device);
        QVERIFY(!e.id.isEmpty());                 // 自动补齐稳定 id
        QCOMPARE(e.config.zoomSpeed, static_cast<quint8>(3)); // 旧字段名 deviceConfig

        QJsonObject legacyGroup{{QStringLiteral("name"), QStringLiteral("旧分组")}};
        QCOMPARE(DeviceEntry::fromJson(legacyGroup).type, DeviceNodeType::Group);
    }

    // ---- 已知 id 不被重写 ----
    void existingIdIsPreserved()
    {
        QJsonObject obj{
            {QStringLiteral("id"), QStringLiteral("dev_keepme")},
            {QStringLiteral("type"), QStringLiteral("device")},
            {QStringLiteral("name"), QStringLiteral("X")},
            {QStringLiteral("ip"), QStringLiteral("1.2.3.4")}
        };
        QCOMPARE(DeviceEntry::fromJson(obj).id, QStringLiteral("dev_keepme"));
    }

    // ---- IP/主机名校验 ----
    void hostValidation_data()
    {
        QTest::addColumn<QString>("host");
        QTest::addColumn<bool>("valid");

        QTest::newRow("ipv4 ok")        << QStringLiteral("192.168.1.10") << true;
        QTest::newRow("ipv4 max")       << QStringLiteral("255.255.255.255") << true;
        QTest::newRow("ipv4 overflow")  << QStringLiteral("256.1.1.1") << false;
        QTest::newRow("ipv4 short")     << QStringLiteral("192.168.1") << false;
        QTest::newRow("hostname")       << QStringLiteral("camera-01.local") << true;
        QTest::newRow("hostname bare")  << QStringLiteral("localhost") << true;
        QTest::newRow("empty")          << QString() << false;
        QTest::newRow("with scheme")    << QStringLiteral("rtsp://192.168.1.1") << false;
        QTest::newRow("with path")      << QStringLiteral("192.168.1.1/live") << false;
        QTest::newRow("with space")     << QStringLiteral("192.168.1.1 ") << false;
        QTest::newRow("leading dash")   << QStringLiteral("-bad") << false;
    }

    void hostValidation()
    {
        QFETCH(QString, host);
        QFETCH(bool, valid);
        QCOMPARE(DeviceEntry::isValidHost(host), valid);
    }

    // ---- RTSP 归一化 ----
    void rtspNormalization()
    {
        QCOMPARE(DeviceEntry::normalizedRtspUrl(QStringLiteral("rtsp://a/b"), QStringLiteral("1.1.1.1")),
                 QStringLiteral("rtsp://a/b"));
        QCOMPARE(DeviceEntry::normalizedRtspUrl(QStringLiteral("  192.168.1.5/live  "), QStringLiteral("1.1.1.1")),
                 QStringLiteral("rtsp://192.168.1.5/live"));
        // 空串回退到默认模板
        QVERIFY(DeviceEntry::normalizedRtspUrl(QString(), QStringLiteral("192.168.1.9"))
                    .contains(QStringLiteral("192.168.1.9")));
    }
};

QTEST_MAIN(TestDeviceEntry)
#include "test_deviceentry.moc"
