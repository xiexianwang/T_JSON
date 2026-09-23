// ============================================================
// test_geocalc.cpp - GeoCalculator 地理算法单元测试
// 覆盖：parseCoord 坐标解析 / haversine 距离 / bearing 航向 /
//       pixelToGps / 视觉测距 / 轨迹抽稀判定
// ============================================================

#include <QtTest>
#include <QtMath>
#include "core/GeoCalculator.h"

class TestGeoCalculator : public QObject
{
    Q_OBJECT
private slots:
    void parseCoord_basic();
    void parseCoord_suffix();
    void parseCoord_invalid();
    void parseCoord_strictOk();

    void haversine_known();
    void haversine_zero();

    void bearing_cardinal();
    void bearing_range();

    void estimateTargetDistance_basic();
    void estimateTargetDistance_invalid();

    void pixelToGps_center();
    void pixelToGps_noPose();

    void shouldPlot_first();
    void shouldPlot_deadZone();
    void shouldPlot_forced();
    void shouldPlot_headingChange();
};

void TestGeoCalculator::parseCoord_basic()
{
    QCOMPARE(GeoCalculator::parseCoord("31.2301"), 31.2301);
    QCOMPARE(GeoCalculator::parseCoord("121.4737"), 121.4737);
    QCOMPARE(GeoCalculator::parseCoord(" 45.5 "), 45.5);
}

void TestGeoCalculator::parseCoord_suffix()
{
    QCOMPARE(GeoCalculator::parseCoord("31.2301N"), 31.2301);
    QCOMPARE(GeoCalculator::parseCoord("31.2301S"), -31.2301);
    QCOMPARE(GeoCalculator::parseCoord("121.4737E"), 121.4737);
    QCOMPARE(GeoCalculator::parseCoord("121.4737W"), -121.4737);
    QCOMPARE(GeoCalculator::parseCoord("45.5n"), 45.5); // 大小写不敏感
}

void TestGeoCalculator::parseCoord_invalid()
{
    QCOMPARE(GeoCalculator::parseCoord("abc"), 0.0);
    QCOMPARE(GeoCalculator::parseCoord(""), 0.0);
    QCOMPARE(GeoCalculator::parseCoord("12N"), 12.0); // 无小数合法
}

void TestGeoCalculator::parseCoord_strictOk()
{
    bool ok = false;
    QCOMPARE(GeoCalculator::parseCoord("0", &ok), 0.0);
    QVERIFY(ok);
    QCOMPARE(GeoCalculator::parseCoord("31.5S", &ok), -31.5);
    QVERIFY(ok);
    QCOMPARE(GeoCalculator::parseCoord("abc", &ok), 0.0);
    QVERIFY(!ok);
    QCOMPARE(GeoCalculator::parseCoord("", &ok), 0.0);
    QVERIFY(!ok);
}

void TestGeoCalculator::haversine_known()
{
    // 北京(39.9042,116.4074) → 上海(31.2304,121.4737)，近似 1067 km
    double dist = GeoCalculator::haversineDistance(39.9042, 116.4074, 31.2304, 121.4737);
    QVERIFY(dist > 1000 * 1000 && dist < 1150 * 1000);
}

void TestGeoCalculator::haversine_zero()
{
    QCOMPARE(GeoCalculator::haversineDistance(31.0, 121.0, 31.0, 121.0), 0.0);
}

void TestGeoCalculator::bearing_cardinal()
{
    // 正北 0°，正东 90°
    QVERIFY(qAbs(GeoCalculator::bearing(0, 0, 1, 0) - 0.0) < 0.5);
    QVERIFY(qAbs(GeoCalculator::bearing(0, 0, 0, 1) - 90.0) < 0.5);
}

void TestGeoCalculator::bearing_range()
{
    // 结果应在 [0, 360)
    double b = GeoCalculator::bearing(0, 0, -1, 0); // 正南 → 180°
    QVERIFY(b >= 0.0 && b < 360.0);
    QVERIFY(qAbs(b - 180.0) < 0.5);
}

void TestGeoCalculator::estimateTargetDistance_basic()
{
    // boxPixels=100, focal=150mm, pixel=4.5um, ref=2m → 2*150*1000/(100*4.5)=666.7
    double d = GeoCalculator::estimateTargetDistance(100, 150.0, 4.5, 2.0);
    QVERIFY(qAbs(d - 666.7) < 1.0);
}

void TestGeoCalculator::estimateTargetDistance_invalid()
{
    QCOMPARE(GeoCalculator::estimateTargetDistance(0, 150.0, 4.5, 2.0), 0.0);
    QCOMPARE(GeoCalculator::estimateTargetDistance(100, 0.0, 4.5, 2.0), 0.0);
    QCOMPARE(GeoCalculator::estimateTargetDistance(100, 150.0, 0.0, 2.0), 0.0);
}

void TestGeoCalculator::pixelToGps_center()
{
    CameraIntrinsics cam{4.5, 150.0, 1920, 1080};
    DevicePose pose{31.0, 121.0, 90.0}; // 朝向正东
    double lat = 0, lon = 0;
    GeoCalculator::pixelToGps(960, 540, 1000.0, cam, pose, lat, lon);
    // 中心像素 → 目标在设备正东 1000m，经度应增大
    QVERIFY(lat != 0 && lon != 0);
    QVERIFY(lon > 121.0);
    QVERIFY(qAbs(lat - 31.0) < 0.02);
}

void TestGeoCalculator::pixelToGps_noPose()
{
    CameraIntrinsics cam{4.5, 150.0, 1920, 1080};
    DevicePose pose{0, 0, 0};
    double lat = 99, lon = 99;
    GeoCalculator::pixelToGps(960, 540, 1000.0, cam, pose, lat, lon);
    QCOMPARE(lat, 0.0);
    QCOMPARE(lon, 0.0);
}

void TestGeoCalculator::shouldPlot_first()
{
    // 首次（plotHeading < 0）必须绘制
    QVERIFY(GeoCalculator::shouldPlotTrackPoint(31.0, 121.0, 31.0, 121.0, -1, QDateTime()));
}

void TestGeoCalculator::shouldPlot_deadZone()
{
    // 距离 < 3m → 不绘制
    QDateTime now = QDateTime::currentDateTime();
    QVERIFY(!GeoCalculator::shouldPlotTrackPoint(31.00001, 121.00001, 31.0, 121.0, 90.0, now));
}

void TestGeoCalculator::shouldPlot_forced()
{
    // 距离 > 20m → 强制绘制
    double heading = -1;
    QVERIFY(GeoCalculator::shouldPlotTrackPoint(31.01, 121.0, 31.0, 121.0, 90.0, QDateTime(), &heading));
    QVERIFY(heading >= 0.0);
}

void TestGeoCalculator::shouldPlot_headingChange()
{
    // 3~20m 之间、航向变化 >= 15° → 绘制
    // 从正北(0°) 走到北偏东约 26.5°（dx=10m,dy=20m 的比例模拟不了精确，改用足够大角度）
    double lat1 = 31.0, lon1 = 121.0;
    double lat2 = 31.0001, lon2 = 121.001; // 明显东偏，航向接近正东
    QVERIFY(GeoCalculator::shouldPlotTrackPoint(lat2, lon2, lat1, lon1, 0.0, QDateTime()));
}

QTEST_GUILESS_MAIN(TestGeoCalculator)
#include "test_geocalc.moc"