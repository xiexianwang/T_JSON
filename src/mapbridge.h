// ========================================================================
// mapbridge.h — C++ ↔ JS 双向通信桥接头文件
// 功能：通过 QWebChannel 在 C++ 与 map.html 页面 JS 之间建立桥梁。
//       JS 侧调用 bridge.onMapInitialized() / bridge.onMapClick()
//       等槽函数；C++ 侧通过发射 jsXxx 信号自动触发 JS 函数执行。
// ========================================================================

#ifndef MAPBRIDGE_H
#define MAPBRIDGE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ========================================================================
// MapBridge — 地图通信桥接对象
// 注册到 QWebChannel 后，JS 可以直接调用其 Q_INVOKABLE 槽函数，
// 同时 C++ 侧发射 jsXxx 信号 → mapwidget.cpp 监听 → runJS() 执行。
//
// 信号命名约定：
//   jsXxx — 由 C++ 发射，最终在 map.html 中执行对应的 jsXxx() 函数
// ========================================================================
class MapBridge : public QObject
{
    Q_OBJECT
    // mapReady 属性：标记地图 JS 侧是否已完成初始化
    Q_PROPERTY(bool mapReady READ mapReady NOTIFY mapReadyChanged)
public:
    explicit MapBridge(QObject *parent = nullptr);

    // 返回地图是否已初始化完成（JS 侧设置）
    bool mapReady() const { return m_ready; }

public slots:
    // ---- JS → C++ 方向 ----
    // 地图页面加载完成时由 JS 调用，标记地图就绪
    void onMapInitialized();
    // 用户单击地图时由 JS 调用，传出经纬度坐标
    void onMapClick(double lat, double lon);

signals:
    // ---- C++ → JS 方向（内部信号，由 mapwidget.cpp 监听） ----
    void mapReadyChanged();                                         // 地图就绪状态变化

    // 用户单击地图（转发到 MapWidget::mapClicked）
    void mapClicked(double lat, double lon);

    void jsDevicePos(double lat, double lon);                       // 设置设备位置标记
    void jsVisFov(double lat, double lon, double pan,               // 绘制可见光 FOV（蓝色）
                  double tilt, double hfov, double vfov, double range);
    void jsIrFov(double lat, double lon, double pan,                // 绘制红外 FOV（红色）
                 double tilt, double hfov, double vfov, double range);
    void jsClearFov();                                              // 清除所有 FOV
    void jsAddTrackPoint(const QString& trackId, double lat, double lon, double speed = 0); // 追加轨迹点（含速度）
    void jsClearTrack(const QString& trackId);                      // 清除单条轨迹
    void jsClearAllTracks();                                        // 清除全部轨迹
    void jsUpdateTargets(const QString& jsonStr);                   // 更新目标标记
    void jsSetMapType(int type);                                    // 切换底图类型
    void jsSetZoom(int level);                                      // 设置缩放级别
    void jsDeviceInfo(double lat, double lon, double alt,           // 更新设备信息面板
                      double pan, double tilt, double hfov, double vfov, double range);

private:
    bool m_ready = false;   // 地图初始化完成标志
};

#endif
