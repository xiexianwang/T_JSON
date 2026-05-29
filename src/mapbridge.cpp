// ========================================================================
// mapbridge.cpp — C++ ↔ JS 双向通信桥接实现
// 提供由 JS 端直接调用的槽函数实现，以及属性管理。
// ========================================================================

#include "mapbridge.h"
#include <QDebug>

MapBridge::MapBridge(QObject *parent)
    : QObject(parent)
{
}

// ---- JS → C++：地图初始化完成回调 ----
// JS 侧在 Leaflet 地图加载完成且 QWebChannel 建立后调用此槽函数，
// 设置 m_ready 标记并通知 Qt 侧地图已就绪。
void MapBridge::onMapInitialized()
{
    m_ready = true;
    emit mapReadyChanged();
    qDebug() << "Map initialized";
}

// ---- JS → C++：地图单击回调 ----
// 用户在地图页面上单击时，JS 侧捕获 Leaflet 的 click 事件，
// 调用此槽函数将经纬度传回 C++ 侧，最终触发 MapWidget::mapClicked 信号。
void MapBridge::onMapClick(double lat, double lon)
{
    emit mapClicked(lat, lon);
}
