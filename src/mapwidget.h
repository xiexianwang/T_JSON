// ========================================================================
// mapwidget.h — 地图控件（Widget）头文件
// 功能：封装 QWebEngineView + Leaflet JS API，提供 C++ 侧的地图操作接口。
//       通过 QWebChannel 与 map.html 中的 JavaScript 双向通信。
// ========================================================================

#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QJsonArray>
#include "mapbridge.h"

class QWebChannel;

namespace Ui {
class MapWidget;
}

// ========================================================================
// MapWidget — 地图容器控件
// 职责：
//   1. 加载 map.html，初始化 Leaflet 地图
//   2. 提供 C++ 接口（设置设备位置、FOV、轨迹、目标标记等）
//   3. 通过 MapBridge（QWebChannel）桥接 JS 与 C++ 之间的信号/槽
// ========================================================================
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);
    ~MapWidget();

    // ---- 设备位置 ----
    // 在地图上标记设备当前位置（GPS 坐标，WGS-84）
    void setDevicePosition(double lat, double lon);

    // ---- 设备视场（FOV） ----
    void setVisFov(double lat, double lon, double panDeg, double tiltDeg,  // 可见光 FOV（蓝色）
                   double hfovDeg, double vfovDeg, double rangeM);
    void setIrFov(double lat, double lon, double panDeg, double tiltDeg,   // 红外 FOV（红色）
                  double hfovDeg, double vfovDeg, double rangeM);
    void clearFov();                                        // 清除所有 FOV

    // ---- 设备信息面板 ----
    // 将设备姿态/位置参数更新到左上角信息面板
    void setDeviceInfo(double lat, double lon, double alt, double pan,
                       double tilt, double hfov, double vfov, double range);
    void setDeviceInfoVisible(bool visible);        // OSD 开关

    // ---- 轨迹管理 ----
    void appendTrackPoint(const QString& trackId, double lat, double lon, double speed = 0); // 追加轨迹点（含速度）
    void clearTrack(const QString& trackId);                               // 清除单条轨迹
    void clearAllTracks();                                                 // 清除全部轨迹

    // ---- 目标标记 ----
    // 将检测到的目标列表以圆形标记绘制到地图上
    void updateTargetMarkers(const QJsonArray& targets);

    // ---- 地图控制 ----
    void setMapType(int type);      // 切换卫星/街道底图
    void setZoom(int level);        // 设置缩放级别
    void reloadMap();               // 重新加载地图页面

signals:
    // 用户在地图上单击时触发，传出 GPS 坐标
    void mapClicked(double lat, double lon);
    // 地图缩放变化时触发
    void mapZoomChanged(int zoom);

private:
    // 辅助：在页面 WebEngine 中执行一段 JavaScript
    void runJS(const QString& js);

    Ui::MapWidget *ui;              // UI 设计器生成的界面
    QWebChannel *m_channel;         // WebChannel，C++ ↔ JS 双向通信
    MapBridge *m_bridge;            // 桥接对象，接收 JS 调用并发射 Qt 信号
};

#endif
