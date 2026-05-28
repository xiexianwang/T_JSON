#include "mapwidget.h"
#include "ui_mapwidget.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QtMath>

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MapWidget)
    , m_channel(new QWebChannel(this))
    , m_bridge(new MapBridge(this))
{
    ui->setupUi(this);

    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    ui->m_webView->page()->setWebChannel(m_channel);

    // Load map HTML with AMap JS API (loaded from CDN)
    QFile htmlF(QStringLiteral(":/map.html"));
    if (htmlF.open(QFile::ReadOnly)) {
        QString html = QString::fromUtf8(htmlF.readAll());
        ui->m_webView->setHtml(html, QUrl(QStringLiteral("https://localhost/")));
    } else {
        qWarning() << "Failed to load map resources";
    }

    // Debug: JS console messages
    auto *page = ui->m_webView->page();
    connect(page, &QWebEnginePage::loadFinished, this, [](bool ok) {
        qDebug() << "[Map] Page load:" << (ok ? "OK" : "FAILED");
    });
    connect(page, &QWebEnginePage::loadProgress, this, [](int p) {
        qDebug() << "[Map] Load progress:" << p << "%";
    });

    // Forward map click
    connect(m_bridge, &MapBridge::mapClicked, this, &MapWidget::mapClicked);

    // Bridge signals → JS calls
    connect(m_bridge, &MapBridge::jsDevicePos, this, [this](double lat, double lon) {
        runJS(QStringLiteral("jsSetDevicePos(%1,%2)").arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8));
    });
    connect(m_bridge, &MapBridge::jsDeviceFov, this, [this](double lat, double lon, double pan, double tilt, double hfov, double vfov, double range) {
        runJS(QStringLiteral("jsSetDeviceFov(%1,%2,%3,%4,%5,%6,%7)")
              .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8)
              .arg(pan, 0, 'f', 4).arg(tilt, 0, 'f', 4)
              .arg(hfov, 0, 'f', 4).arg(vfov, 0, 'f', 4)
              .arg(range, 0, 'f', 1));
    });
    connect(m_bridge, &MapBridge::jsClearFov, this, [this]() {
        runJS(QStringLiteral("jsClearFov()"));
    });
    connect(m_bridge, &MapBridge::jsAddTrackPoint, this, [this](const QString& id, double lat, double lon) {
        runJS(QStringLiteral("jsAddTrackPoint('%1',%2,%3)")
              .arg(id).arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8));
    });
    connect(m_bridge, &MapBridge::jsClearTrack, this, [this](const QString& id) {
        runJS(QStringLiteral("jsClearTrack('%1')").arg(id));
    });
    connect(m_bridge, &MapBridge::jsClearAllTracks, this, [this]() {
        runJS(QStringLiteral("jsClearAllTracks()"));
    });
    connect(m_bridge, &MapBridge::jsUpdateTargets, this, [this](const QString& json) {
        runJS(QStringLiteral("jsUpdateTargets('%1')").arg(json));
    });
    connect(m_bridge, &MapBridge::jsSetMapType, this, [this](int type) {
        if (type == 0)
            runJS(QStringLiteral("jsSetSatelliteMap()"));
        else
            runJS(QStringLiteral("jsSetStreetMap()"));
    });
    connect(m_bridge, &MapBridge::jsSetZoom, this, [this](int level) {
        runJS(QStringLiteral("jsSetZoom(%1)").arg(level));
    });
    connect(m_bridge, &MapBridge::jsDeviceInfo, this, [this](double lat, double lon, double alt, double pan, double tilt, double hfov, double vfov, double range) {
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

void MapWidget::runJS(const QString& js)
{
    ui->m_webView->page()->runJavaScript(js);
}

void MapWidget::setDevicePosition(double lat, double lon)
{
    emit m_bridge->jsDevicePos(lat, lon);
}

void MapWidget::setDeviceInfo(double lat, double lon, double alt, double pan, double tilt, double hfov, double vfov, double range)
{
    emit m_bridge->jsDeviceInfo(lat, lon, alt, pan, tilt, hfov, vfov, range);
}

void MapWidget::setDeviceFov(double lat, double lon, double panDeg, double tiltDeg,
                              double hfovDeg, double vfovDeg, double rangeM)
{
    emit m_bridge->jsDeviceFov(lat, lon, panDeg, tiltDeg, hfovDeg, vfovDeg, rangeM);
}

void MapWidget::clearFov()
{
    emit m_bridge->jsClearFov();
}

void MapWidget::appendTrackPoint(const QString& trackId, double lat, double lon)
{
    emit m_bridge->jsAddTrackPoint(trackId, lat, lon);
}

void MapWidget::clearTrack(const QString& trackId)
{
    emit m_bridge->jsClearTrack(trackId);
}

void MapWidget::clearAllTracks()
{
    emit m_bridge->jsClearAllTracks();
}

void MapWidget::updateTargetMarkers(const QJsonArray& targets)
{
    QJsonDocument doc(targets);
    QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    emit m_bridge->jsUpdateTargets(json);
}

void MapWidget::setMapType(int type)
{
    emit m_bridge->jsSetMapType(type);
}

void MapWidget::setZoom(int level)
{
    emit m_bridge->jsSetZoom(level);
}
