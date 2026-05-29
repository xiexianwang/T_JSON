// ========================================================================
// mapwidget.cpp — 地图控件（Widget）实现
// 功能：初始化 WebEngine，加载 Leaflet 地图页面；通过 QWebChannel 桥接
//       C++ 与 JS，所有地图操作均转为 JS 调用在页面中执行。
// ========================================================================

#include "mapwidget.h"
#include "ui_mapwidget.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QtMath>

// ========================================================================
// 构造函数
// 1. 创建 QWebChannel，注册 MapBridge 为 "bridge" 对象
// 2. 从 Qt 资源文件加载 map.html 到 WebEngine
// 3. 连接 Bridge 的 JS 请求信号 → runJS 执行对应 JS 函数
// ========================================================================
MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MapWidget)
    , m_channel(new QWebChannel(this))
    , m_bridge(new MapBridge(this))
{
    ui->setupUi(this);

    // 将 bridge 对象注册到 WebChannel，JS 侧可通过 bridge 对象调用其槽函数
    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    ui->m_webView->page()->setWebChannel(m_channel);

    // 从 Qt 资源系统加载 Leaflet 地图 HTML
    QFile htmlF(QStringLiteral(":/map.html"));
    if (htmlF.open(QFile::ReadOnly)) {
        QString html = QString::fromUtf8(htmlF.readAll());
        ui->m_webView->setHtml(html);
    } else {
        qWarning() << "Failed to load map resources";
    }

    // 调试：监听页面加载进度与完成状态
    auto *page = ui->m_webView->page();
    connect(page, &QWebEnginePage::loadFinished, this, [](bool ok) {
        qDebug() << "[Map] Page load:" << (ok ? "OK" : "FAILED");
    });
    connect(page, &QWebEnginePage::loadProgress, this, [](int p) {
        qDebug() << "[Map] Load progress:" << p << "%";
    });

    // 将 Bridge 收到的地图单击信号转发到 MapWidget 的同名信号
    connect(m_bridge, &MapBridge::mapClicked, this, &MapWidget::mapClicked);

    // ---- Bridge 信号 → 执行 JS ----
    // 每个信号对应 map.html 中一个 jsXxx 函数

    // 设置设备位置标记
    connect(m_bridge, &MapBridge::jsDevicePos, this, [this](double lat, double lon) {
        runJS(QStringLiteral("jsSetDevicePos(%1,%2)").arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8));
    });
    // 绘制可见光 FOV（蓝色锥形）
    connect(m_bridge, &MapBridge::jsVisFov, this, [this](double lat, double lon,
            double pan, double tilt, double hfov, double vfov, double range) {
        runJS(QStringLiteral("jsSetVisFov(%1,%2,%3,%4,%5,%6,%7)")
              .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8)
              .arg(pan, 0, 'f', 4).arg(tilt, 0, 'f', 4)
              .arg(hfov, 0, 'f', 4).arg(vfov, 0, 'f', 4)
              .arg(range, 0, 'f', 1));
    });
    // 绘制红外 FOV（红色锥形）
    connect(m_bridge, &MapBridge::jsIrFov, this, [this](double lat, double lon,
            double pan, double tilt, double hfov, double vfov, double range) {
        runJS(QStringLiteral("jsSetIrFov(%1,%2,%3,%4,%5,%6,%7)")
              .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8)
              .arg(pan, 0, 'f', 4).arg(tilt, 0, 'f', 4)
              .arg(hfov, 0, 'f', 4).arg(vfov, 0, 'f', 4)
              .arg(range, 0, 'f', 1));
    });
    // 清除所有 FOV
    connect(m_bridge, &MapBridge::jsClearFov, this, [this]() {
        runJS(QStringLiteral("jsClearFov()"));
    });
    // 追加轨迹点
    connect(m_bridge, &MapBridge::jsAddTrackPoint, this, [this](const QString& id, double lat, double lon) {
        runJS(QStringLiteral("jsAddTrackPoint('%1',%2,%3)")
              .arg(id).arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8));
    });
    // 清除单条轨迹
    connect(m_bridge, &MapBridge::jsClearTrack, this, [this](const QString& id) {
        runJS(QStringLiteral("jsClearTrack('%1')").arg(id));
    });
    // 清除全部轨迹
    connect(m_bridge, &MapBridge::jsClearAllTracks, this, [this]() {
        runJS(QStringLiteral("jsClearAllTracks()"));
    });
    // 更新目标标记（JSON 序列化后传给 JS）
    connect(m_bridge, &MapBridge::jsUpdateTargets, this, [this](const QString& json) {
        runJS(QStringLiteral("jsUpdateTargets('%1')").arg(json));
    });
    // 切换底图类型：0=卫星，非0=街道
    connect(m_bridge, &MapBridge::jsSetMapType, this, [this](int type) {
        if (type == 0)
            runJS(QStringLiteral("jsSetSatelliteMap()"));
        else
            runJS(QStringLiteral("jsSetStreetMap()"));
    });
    // 设置缩放级别
    connect(m_bridge, &MapBridge::jsSetZoom, this, [this](int level) {
        runJS(QStringLiteral("jsSetZoom(%1)").arg(level));
    });
    // 更新设备信息面板（左上角数据显示）
    connect(m_bridge, &MapBridge::jsDeviceInfo, this, [this](double lat, double lon,
            double alt, double pan, double tilt, double hfov, double vfov, double range) {
        runJS(QStringLiteral("jsSetDeviceInfo(%1,%2,%3,%4,%5,%6,%7,%8)")
              .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6)
              .arg(alt, 0, 'f', 1).arg(pan, 0, 'f', 1)
              .arg(tilt, 0, 'f', 1).arg(hfov, 0, 'f', 1)
              .arg(vfov, 0, 'f', 1).arg(range, 0, 'f', 0));
    });
}

MapWidget::~MapWidget()
{
    delete ui;
}

// 在 WebEngine 页面中执行 JavaScript 代码
void MapWidget::runJS(const QString& js)
{
    ui->m_webView->page()->runJavaScript(js);
}

// 设置设备位置：通过 Bridge 信号 → 自动触发 JS 侧绘制标记
void MapWidget::setDevicePosition(double lat, double lon)
{
    emit m_bridge->jsDevicePos(lat, lon);
}

// 更新设备信息面板
void MapWidget::setDeviceInfo(double lat, double lon, double alt, double pan,
                               double tilt, double hfov, double vfov, double range)
{
    emit m_bridge->jsDeviceInfo(lat, lon, alt, pan, tilt, hfov, vfov, range);
}

// 设置可见光视场角覆盖范围（蓝色锥形）
void MapWidget::setVisFov(double lat, double lon, double panDeg, double tiltDeg,
                           double hfovDeg, double vfovDeg, double rangeM)
{
    emit m_bridge->jsVisFov(lat, lon, panDeg, tiltDeg, hfovDeg, vfovDeg, rangeM);
}

// 设置红外视场角覆盖范围（红色锥形）
void MapWidget::setIrFov(double lat, double lon, double panDeg, double tiltDeg,
                          double hfovDeg, double vfovDeg, double rangeM)
{
    emit m_bridge->jsIrFov(lat, lon, panDeg, tiltDeg, hfovDeg, vfovDeg, rangeM);
}

// 清除所有视场角
void MapWidget::clearFov()
{
    emit m_bridge->jsClearFov();
}

// 向指定轨迹追加一个坐标点
void MapWidget::appendTrackPoint(const QString& trackId, double lat, double lon)
{
    emit m_bridge->jsAddTrackPoint(trackId, lat, lon);
}

// 清除指定 ID 的轨迹
void MapWidget::clearTrack(const QString& trackId)
{
    emit m_bridge->jsClearTrack(trackId);
}

// 清除所有轨迹
void MapWidget::clearAllTracks()
{
    emit m_bridge->jsClearAllTracks();
}

// 更新目标标记：将 QJsonArray 序列化为 JSON 字符串后发给 JS
void MapWidget::updateTargetMarkers(const QJsonArray& targets)
{
    QJsonDocument doc(targets);
    QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    emit m_bridge->jsUpdateTargets(json);
}

// 切换底图类型（卫星/街道）
void MapWidget::setMapType(int type)
{
    emit m_bridge->jsSetMapType(type);
}

// 设置地图缩放级别
void MapWidget::setZoom(int level)
{
    emit m_bridge->jsSetZoom(level);
}
