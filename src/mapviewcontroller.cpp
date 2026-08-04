#include "mapviewcontroller.h"
#include "mapwidget.h"
#include "videogridwidget.h"
#include "videowidget.h"
#include "devicemanager.h"
#include "trackmanager.h"
#include "configmanager.h"
#include "geoutils.h"
#include "jsonframeparser.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QMouseEvent>
#include <QRegion>
#include <QtMath>

MapViewController::MapViewController(QWidget *displayContainer,
                                     VideoGridWidget *videoGrid,
                                     QWidget *drawerPanel,
                                     QPushButton *drawerToggleBtn,
                                     DeviceManager *devMgr,
                                     TrackManager *trackMgr,
                                     ConfigManager *cfg,
                                     QObject *parent)
    : QObject(parent)
    , m_videoGrid(videoGrid)
    , m_drawerPanel(drawerPanel)
    , m_drawerToggleBtn(drawerToggleBtn)
    , m_devMgr(devMgr)
    , m_trackMgr(trackMgr)
    , m_cfg(cfg)
{
    createMapUi(displayContainer);
    createPipUi();

    connect(m_mapWidget, &MapWidget::miniRequested, this, [this]() { toggleMode(); });
    connect(m_mapWidget, &MapWidget::closeRequested, this, [this]() { toggleVisibility(); });
    connect(m_mapWidget, &MapWidget::enlargeRequested, this, [this]() { toggleMode(); });
}

void MapViewController::createMapUi(QWidget *displayContainer)
{
    m_mapContainer = new QWidget(displayContainer);
    m_mapContainer->setVisible(false);
    m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);

    m_mapWidget = new MapWidget(m_mapContainer);
    m_mapWidget->setGeometry(0, 0, 280, 280);

    m_mapOverlay = new QWidget(m_mapContainer);
    m_mapOverlay->setGeometry(0, 0, 280, 280);
    m_mapOverlay->setCursor(Qt::OpenHandCursor);
    m_mapOverlay->installEventFilter(this);
}

void MapViewController::createPipUi()
{
    if (m_pipDialog) return;
    m_pipDialog = new QDialog(static_cast<QWidget*>(parent()), Qt::FramelessWindowHint | Qt::Tool);
    m_pipDialog->setAttribute(Qt::WA_ShowWithoutActivating);
    m_pipDialog->setFixedSize(320, 200);
    auto *pipLay = new QVBoxLayout(m_pipDialog);
    pipLay->setSpacing(0);
    pipLay->setContentsMargins(0, 0, 0, 0);

    m_pipTitle = new QWidget(m_pipDialog);
    m_pipTitle->setFixedHeight(22);
    m_pipTitle->setCursor(Qt::OpenHandCursor);
    m_pipTitle->installEventFilter(this);
    m_pipTitle->setStyleSheet(QStringLiteral(
        "background-color:#1a1a2e; border-bottom:1px solid #16213e;"
    ));
    auto *titleLay = new QHBoxLayout(m_pipTitle);
    titleLay->setContentsMargins(0, 0, 2, 0);
    titleLay->addStretch();
    auto *btnClose = new QPushButton(QStringLiteral("\u2715"), m_pipTitle);
    btnClose->setFixedSize(20, 20);
    btnClose->setCursor(Qt::ArrowCursor);
    btnClose->setStyleSheet(QStringLiteral(
        "QPushButton{background:transparent;color:#a0a0b0;border:none;font-size:13px;}"
        "QPushButton:hover{background:#e74c3c;color:#fff;border-radius:2px;}"
    ));
    connect(btnClose, &QPushButton::clicked, this, [this]() { m_pipDialog->hide(); });
    pipLay->addWidget(m_pipTitle);

    m_pipVideo = new VideoWidget(m_pipDialog);
    pipLay->addWidget(m_pipVideo);
}

void MapViewController::pipSetFrame(const QImage &frame)
{
    if (m_pipVideo) m_pipVideo->setFrame(frame);
}

void MapViewController::toggleVisibility()
{
    m_mapVisible = !m_mapVisible;
    if (m_mapVisible) {
        m_mapExpanded = false;
        updateLayout();
    } else {
        if (m_pipShown)
            m_pipDialog->hide();
        m_mapContainer->setVisible(false);
        m_videoGrid->setVisible(true);
        m_drawerPanel->setVisible(true);
        m_drawerToggleBtn->setVisible(true);
    }
    emit visibilityChanged(m_mapVisible);
}

void MapViewController::toggleMode()
{
    m_mapExpanded = !m_mapExpanded;
    updateLayout();
    emit modeChanged(m_mapExpanded);
}

void MapViewController::updateLayout()
{
    QWidget *disp = m_mapContainer->parentWidget();
    if (!disp) return;
    QSize ps = disp->size();
    if (ps.isEmpty()) return;

    if (!m_mapVisible) {
        m_videoGrid->setVisible(true);
        m_drawerPanel->setVisible(true);
        m_drawerToggleBtn->setVisible(true);
        m_mapContainer->setVisible(false);
        return;
    }

    m_mapContainer->setVisible(true);

    if (m_mapExpanded) {
        m_videoGrid->setVisible(false);
        m_drawerPanel->setVisible(false);
        m_drawerToggleBtn->setVisible(false);

        m_mapContainer->setGeometry(0, 0, ps.width(), ps.height());
        m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, false);
        m_mapContainer->clearMask();
        m_mapWidget->setGeometry(0, 0, ps.width(), ps.height());
        m_mapWidget->setCircularClip(false);
        m_mapOverlay->setVisible(false);

        m_pipPos = QPoint(8, ps.height() - 200 - 8);
        m_pipDialog->move(m_pipPos);
        m_pipDialog->show();
        m_pipShown = true;
    } else {
        m_videoGrid->setVisible(true);
        m_drawerPanel->setVisible(m_drawerPanel->isVisible());
        m_drawerToggleBtn->setVisible(true);
        if (m_pipShown)
            m_pipDialog->hide();

        int x = m_drawerPanel->isVisible() ? 240 : 0;
        m_drawerToggleBtn->move(x, (ps.height() - m_drawerToggleBtn->height()) / 2);

        m_mapContainer->setGeometry(m_miniMapPos.x(), m_miniMapPos.y(), 280, 280);
        m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);
        m_mapContainer->setMask(QRegion(0, 0, 280, 280, QRegion::Ellipse));
        m_mapWidget->setGeometry(0, 0, 280, 280);
        m_mapOverlay->setGeometry(0, 0, 280, 280);
        m_mapOverlay->setVisible(true);
    }
    m_mapContainer->raise();
}

void MapViewController::updateDevicePosition(const QJsonObject &doc,
                                              double visZoom, double irZoom,
                                              double resX, double resY,
                                              const CameraConfig &cam,
                                              double aiDist, bool aiDistEstimated)
{
    QString latStr = doc.value("Latitude").toString();
    QString lonStr = doc.value("Longitude").toString();
    double lat = GeoUtils::parseCoord(latStr);
    double lon = GeoUtils::parseCoord(lonStr);
    double alt = doc.value("Height").toDouble(0);
    double pan = doc.value("PTZInfoH").toDouble(0);
    double tilt = doc.value("PTZInfoV").toDouble(0);
    double range = doc.value("LaserRange").toDouble(0);
    if (range <= 0) {
        range = aiDist;
    }

    qDebug() << "[MapPos] raw:" << latStr << lonStr << "parsed:" << lat << lon;
    if (lat == 0 && lon == 0) return;

    m_mapWidget->setDevicePosition(lat, lon);

    double visSensorW = cam.visPixelSize * resX / 1000.0;
    double visFocal = cam.visMinFocal * visZoom;
    double visHfov = 2.0 * qAtan(visSensorW / (2.0 * visFocal)) * 180.0 / M_PI;
    double visVfov = visHfov * resY / resX;

    double irSensorW = cam.irPixelSize * cam.irResX / 1000.0;
    double irFocal = cam.irMinFocal * irZoom;
    double irHfov = 2.0 * qAtan(irSensorW / (2.0 * irFocal)) * 180.0 / M_PI;
    double irVfov = irHfov * cam.irResY / cam.irResX;

    m_mapWidget->setVisFov(lat, lon, pan, tilt, visHfov, visVfov, 4000);
    m_mapWidget->setIrFov(lat, lon, pan, tilt, irHfov, irVfov, 2000);
    m_mapWidget->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, aiDistEstimated);
}

void MapViewController::updateTargets(const QJsonObject &doc, int workMode,
                                      double devLat, double devLon,
                                      double pan, double tilt,
                                      double px, double focal,
                                      int halfW, int halfH, int algoModel,
                                      double visZoom, double irZoom, int pipShow,
                                      const CameraConfig &cam)
{
    if (workMode == 1) {
        AiInfoData ai = AiInfoData::parse(doc);

        if (ai.targets.isEmpty()) {
            m_mapWidget->clearAllTracks();
            m_mapWidget->clearFov();
            m_mapWidget->updateTargetMarkers(QJsonArray());
            return;
        }

        QJsonArray targetArr;
        for (const auto& aiT : ai.targets) {
            if (!aiT.hasPoints) continue;
            double tLat = 0, tLon = 0;
            double cx = (aiT.left + aiT.right) / 2.0;
            double cy = (aiT.top + aiT.bottom) / 2.0;
            GeoUtils::pixelToGps(cx, cy, aiT.distance, devLat, devLon, pan,
                                 px, focal, halfW, halfH, tLat, tLon);

            QJsonArray bbox;
            double bLat, bLon;
            GeoUtils::pixelBboxToGps(aiT.left, aiT.top, aiT.distance, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
            GeoUtils::pixelBboxToGps(aiT.right, aiT.top, aiT.distance, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
            GeoUtils::pixelBboxToGps(aiT.right, aiT.bottom, aiT.distance, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
            GeoUtils::pixelBboxToGps(aiT.left, aiT.bottom, aiT.distance, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});

            QJsonObject t;
            t[QStringLiteral("id")] = aiT.id;
            t[QStringLiteral("cls")] = aiT.cls;
            t[QStringLiteral("dist")] = aiT.distance;
            t[QStringLiteral("lat")] = tLat;
            t[QStringLiteral("lon")] = tLon;
            t[QStringLiteral("locked")] = false;
            t[QStringLiteral("bbox")] = bbox;
            targetArr.append(t);
        }
        m_mapWidget->updateTargetMarkers(targetArr);
        return;
    }

    if (workMode >= 2 && workMode <= 4) {
        AiInfoData ai = AiInfoData::parse(doc);
        auto result = m_trackMgr->processAiFrame(
            ai, workMode, devLat, devLon, pan, tilt,
            px, focal, halfW, halfH,
            cam, algoModel,
            visZoom, irZoom, pipShow);

        if (result.cleared) {
            m_mapWidget->clearAllTracks();
            m_mapWidget->updateTargetMarkers(QJsonArray());
            return;
        }

        if (result.shouldPlot) {
            m_mapWidget->appendTrackPoint(result.id, result.lat, result.lon, result.speed);
        }

        if (result.hasLock || result.hasLost) {
            QJsonArray targetArr;
            QJsonObject t;
            double dist = result.dist;
            double tLat = result.lat, tLon = result.lon;

            if (result.hasLock) {
                int L = 0, T = 0, R = 0, B = 0;
                for (const auto& aiT : ai.targets) {
                    if (aiT.id == result.id && aiT.hasPoints) {
                        L = aiT.left; T = aiT.top;
                        R = aiT.right; B = aiT.bottom;
                        break;
                    }
                }

                QJsonArray bbox;
                double bLat, bLon;
                GeoUtils::pixelBboxToGps(L, T, dist, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoUtils::pixelBboxToGps(R, T, dist, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoUtils::pixelBboxToGps(R, B, dist, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoUtils::pixelBboxToGps(L, B, dist, tilt, devLat, devLon, pan, px, focal, halfW, halfH, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                t[QStringLiteral("bbox")] = bbox;
            }

            t[QStringLiteral("id")] = result.id;
            t[QStringLiteral("cls")] = result.cls;
            t[QStringLiteral("dist")] = dist;
            t[QStringLiteral("lat")] = tLat;
            t[QStringLiteral("lon")] = tLon;
            t[QStringLiteral("locked")] = result.hasLock;
            t[QStringLiteral("speed")] = result.speed;
            targetArr.append(t);
            m_mapWidget->updateTargetMarkers(targetArr);
        }
    }
}

bool MapViewController::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_mapOverlay) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStart = me->pos();
                m_dragging = false;
            }
            return true;
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                QPoint delta = me->pos() - m_dragStart;
                if (delta.manhattanLength() > 5) {
                    m_dragging = true;
                    m_miniMapPos = m_mapContainer->pos() + delta;
                    m_mapContainer->move(m_miniMapPos);
                }
            }
            return true;
        }
        case QEvent::MouseButtonRelease: {
            m_dragging = false;
            return true;
        }
        case QEvent::MouseButtonDblClick: {
            toggleMode();
            return true;
        }
        default:
            break;
        }
    }

    if (obj == m_pipTitle) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_pipDragStart = me->globalPosition().toPoint();
                m_pipTitle->setCursor(Qt::ClosedHandCursor);
            }
            return true;
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                m_pipPos = m_pipDialog->pos() + (me->globalPosition().toPoint() - m_pipDragStart);
                m_pipDialog->move(m_pipPos);
                m_pipDragStart = me->globalPosition().toPoint();
            }
            return true;
        }
        case QEvent::MouseButtonRelease: {
            m_pipTitle->setCursor(Qt::OpenHandCursor);
            return true;
        }
        default:
            break;
        }
    }

    return QObject::eventFilter(obj, event);
}
