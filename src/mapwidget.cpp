#include "mapwidget.h"
#include "ui_mapwidget.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QtMath>

namespace {
QString jsEscape(const QString& s)
{
    QString r;
    r.reserve(s.size());
    for (QChar c : s) {
        switch (c.unicode()) {
            case '\\': r += QStringLiteral("\\\\"); break;
            case '\'': r += QStringLiteral("\\'"); break;
            case '\n': r += QStringLiteral("\\n"); break;
            case '\r': r += QStringLiteral("\\r"); break;
            case '\t': r += QStringLiteral("\\t"); break;
            default:   r += c; break;
        }
    }
    return r;
}
}

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MapWidget)
    , m_aiFlushTimer(new QTimer(this))
    , m_channel(new QWebChannel(this))
    , m_bridge(new MapBridge(this))
{
    ui->setupUi(this);
    m_aiFlushTimer->setInterval(200);
    connect(m_aiFlushTimer, &QTimer::timeout, this, &MapWidget::flushAiData);
    m_aiFlushTimer->start();

    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    auto *page = ui->m_webView->page();
    page->setWebChannel(m_channel);
    page->setBackgroundColor(Qt::transparent);

    QFile htmlF(QStringLiteral(":/map.html"));
    if (htmlF.open(QFile::ReadOnly)) {
        QString html = QString::fromUtf8(htmlF.readAll());
        ui->m_webView->setHtml(html);
    } else {
        qWarning() << "Failed to load map resources";
    }

    connect(page, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        qDebug() << "[Map] Page load:" << (ok ? "OK" : "FAILED");
        if (ok && m_pendingReload) {
            m_pendingReload = false;
            setZoom(m_pendingZoom);
            setMapType(m_pendingMapType);
        }
    });
    connect(page, &QWebEnginePage::loadProgress, this, [](int p) {
        qDebug() << "[Map] Load progress:" << p << "%";
    });

    connect(m_bridge, &MapBridge::mapClicked, this, &MapWidget::mapClicked);
    connect(m_bridge, &MapBridge::mapZoomChanged, this, &MapWidget::mapZoomChanged);
    connect(m_bridge, &MapBridge::requestEnlarge, this, &MapWidget::enlargeRequested);

    // 工具栏内部信号连接
    connect(ui->spinZoom, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MapWidget::setZoom);
    connect(this, &MapWidget::mapZoomChanged,
            this, [this](int zoom) { QSignalBlocker _(ui->spinZoom); ui->spinZoom->setValue(zoom); });
    connect(ui->chkDeviceInfo, &QCheckBox::toggled,
            this, &MapWidget::setDeviceInfoVisible);
    connect(ui->comboDetailLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MapWidget::setDetailLevel);
    connect(ui->btnRefresh, &QPushButton::clicked,
            this, [this]() { reloadMap(); setPendingState(ui->spinZoom->value(), 0); });
    connect(ui->btnMini, &QPushButton::clicked,
            this, &MapWidget::miniRequested);
    connect(ui->btnClose, &QPushButton::clicked,
            this, &MapWidget::closeRequested);
}

MapWidget::~MapWidget()
{
    qDebug() << "[Map] MapWidget destructing";
    m_aiFlushTimer->stop();
    delete ui;
}

QWebEngineView* MapWidget::webView() const
{
    return ui->m_webView;
}

void MapWidget::runJS(const QString& js)
{
    ui->m_webView->page()->runJavaScript(js);
}

// ── ZoomInfo 数据（设备200ms推送一次）：直接runJS ──

void MapWidget::setDevicePosition(double lat, double lon)
{
    runJS(QStringLiteral("jsSetDevicePos(%1,%2)").arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8));
}

void MapWidget::setDeviceInfo(double lat, double lon, double alt, double pan,
                               double tilt, double hfov, double vfov, double range,
                               bool estimated)
{
    runJS(QStringLiteral("jsSetDeviceInfo(%1,%2,%3,%4,%5,%6,%7,%8,%9)")
          .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6)
          .arg(alt, 0, 'f', 1).arg(pan, 0, 'f', 1)
          .arg(tilt, 0, 'f', 1).arg(hfov, 0, 'f', 1)
          .arg(vfov, 0, 'f', 1).arg(range, 0, 'f', 0)
          .arg(estimated ? QStringLiteral("true") : QStringLiteral("false")));
}

void MapWidget::setVisFov(double lat, double lon, double panDeg, double tiltDeg,
                           double hfovDeg, double vfovDeg, double rangeM)
{
    runJS(QStringLiteral("jsSetVisFov(%1,%2,%3,%4,%5,%6,%7)")
          .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8)
          .arg(panDeg, 0, 'f', 4).arg(tiltDeg, 0, 'f', 4)
          .arg(hfovDeg, 0, 'f', 4).arg(vfovDeg, 0, 'f', 4)
          .arg(rangeM, 0, 'f', 1));
}

void MapWidget::setIrFov(double lat, double lon, double panDeg, double tiltDeg,
                          double hfovDeg, double vfovDeg, double rangeM)
{
    runJS(QStringLiteral("jsSetIrFov(%1,%2,%3,%4,%5,%6,%7)")
          .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8)
          .arg(panDeg, 0, 'f', 4).arg(tiltDeg, 0, 'f', 4)
          .arg(hfovDeg, 0, 'f', 4).arg(vfovDeg, 0, 'f', 4)
          .arg(rangeM, 0, 'f', 1));
}

void MapWidget::clearFov()
{
    runJS(QStringLiteral("jsClearFov()"));
}

void MapWidget::flushAiData()
{
    if (m_dirtyFlags & DIRTY_TRK_CLR) {
        runJS(QStringLiteral("jsClearAllTracks()"));
    } else if (m_dirtyFlags & DIRTY_TRACKS) {
        for (const auto& pt : m_cacheTrackPts) {
            runJS(QStringLiteral("jsAddTrackPoint('%1',%2,%3,%4)")
                  .arg(jsEscape(pt.id)).arg(pt.lat, 0, 'f', 8)
                  .arg(pt.lon, 0, 'f', 8).arg(pt.speed, 0, 'f', 2));
        }
        m_cacheTrackPts.clear();
    }
    if (m_dirtyFlags & DIRTY_TARGETS) {
        QJsonDocument doc(m_cacheTargets);
        QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        runJS(QStringLiteral("jsUpdateTargets('%1')").arg(jsEscape(json)));
    }
    m_dirtyFlags = 0;
}

void MapWidget::appendTrackPoint(const QString& trackId, double lat, double lon, double speed)
{
    m_cacheTrackPts.append({trackId, lat, lon, speed});
    m_dirtyFlags |= DIRTY_TRACKS;
}

void MapWidget::clearTrack(const QString& trackId)
{
    runJS(QStringLiteral("jsClearTrack('%1')").arg(jsEscape(trackId)));
}

void MapWidget::clearAllTracks()
{
    m_cacheTrackPts.clear();
    m_dirtyFlags |= DIRTY_TRK_CLR;
    m_dirtyFlags &= ~DIRTY_TRACKS;
}

void MapWidget::updateTargetMarkers(const QJsonArray& targets)
{
    m_cacheTargets = targets;
    m_dirtyFlags |= DIRTY_TARGETS;
}

void MapWidget::setMapType(int type)
{
    if (type == 0)
        runJS(QStringLiteral("jsSetSatelliteMap()"));
    else
        runJS(QStringLiteral("jsSetStreetMap()"));
    QSignalBlocker _(ui->comboDetailLevel);
    ui->comboDetailLevel->setCurrentIndex(type == 0 ? 1 : 3);
}

void MapWidget::setZoom(int level)
{
    runJS(QStringLiteral("jsSetZoom(%1)").arg(level));
}

void MapWidget::setDeviceInfoVisible(bool visible)
{
    runJS(QStringLiteral("jsToggleDeviceInfo(%1)").arg(visible ? "true" : "false"));
}

void MapWidget::reloadMap()
{
    qDebug() << "[Map] Reloading map page...";
    m_pendingReload = true;
    ui->m_webView->reload();
}

void MapWidget::setCircularClip(bool enabled, double lat, double lon, int zoom)
{
    if (enabled) {
        runJS(QStringLiteral("jsSetMiniShape(%1,%2,%3)")
              .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8).arg(zoom));
    } else {
        runJS(QStringLiteral("jsSetFullShape()"));
    }
    ui->toolbar->setVisible(!enabled);
    if (enabled) {
        setDeviceInfoVisible(false);
    } else if (ui->chkDeviceInfo->isChecked()) {
        setDeviceInfoVisible(true);
    }
}

void MapWidget::setPendingState(int zoom, int mapType)
{
    if (m_pendingReload) {
        m_pendingZoom = zoom;
        m_pendingMapType = mapType;
    }
}

void MapWidget::recenterMap(double lat, double lon, int zoom)
{
    runJS(QStringLiteral("jsRecenterMap(%1,%2,%3)")
          .arg(lat, 0, 'f', 8).arg(lon, 0, 'f', 8).arg(zoom));
}

void MapWidget::setDetailLevel(int level)
{
    runJS(QStringLiteral("jsSetDetailLevel(%1)").arg(level));
}




