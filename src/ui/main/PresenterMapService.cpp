#include "PresenterMapService.h"
#include "ui/main/IMainView.h"
#include "infrastructure/configmanager.h"
#include "core/DeviceState.h"
#include "core/GeoCalculator.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"

#include <QJsonArray>
#include <QPoint>
#include <QtMath>

PresenterMapService::PresenterMapService(IMainView* view, ConfigManager* cfg, QObject* parent)
    : QObject(parent), m_view(view), m_cfg(cfg)
{
}

// 获取指定设备的相机/云台配置；无上下文时回退到默认值
const DeviceConfig& PresenterMapService::deviceCam(const QString& deviceId) const
{
    static const DeviceConfig kDefaultCfg;
    auto* ctx = DeviceManager::instance().getDevice(deviceId);
    return ctx ? ctx->deviceConfig() : kDefaultCfg;
}

PresenterMapService::DeviceMapState& PresenterMapService::mapState(const QString& deviceId)
{
    return m_deviceStates[deviceId];
}

void PresenterMapService::resetDevice(const QString& deviceId)
{
    m_deviceStates.remove(deviceId);
}

double PresenterMapService::calculateVisualDistance(const QString& deviceId,
                                                    const QJsonObject& obj, int cls,
                                                    const DeviceState& state,
                                                    bool updateTrackLabel)
{
    double dist = obj.value("Distance").toDouble(0);
    if (dist > 0 || !obj.contains("Points"))
        return dist;

    int low = state.model % 10;
    const DeviceConfig& cam = deviceCam(deviceId);
    double ref = cam.targetRefSize(low, cls);
    if (ref <= 0) {
        // Class 为真实类型码（0xA0-0xA4）；参考尺寸缺失时按当前模型遍历
        // 其他已配置类型兜底（0xA0 空中目标等无专属参考尺寸）。
        static const int fallback[] = {0xA1, 0xA2, 0xA3, 0xA4};
        for (int fc : fallback) {
            ref = cam.targetRefSize(low, fc);
            if (ref > 0) break;
        }
    }
    if (ref <= 0)
        return dist;

    const QJsonObject pts = obj.value("Points").toObject();
    const int boxPx = qMax(pts.value("Right").toInt() - pts.value("Left").toInt(),
                           pts.value("Bottom").toInt() - pts.value("Top").toInt());
    if (boxPx <= 0)
        return dist;

    const bool isVis = (DeviceState::pipShowToComboIndex(state.currentPipShow) != 1 &&
                        DeviceState::pipShowToComboIndex(state.currentPipShow) != 4);
    const double pxSize = isVis ? cam.visPixelSize : cam.irPixelSize;
    const double focal = isVis ? cam.visMinFocal * state.currentVisZoom
                               : cam.irMinFocal * state.currentIrZoom;
    dist = GeoCalculator::estimateTargetDistance(boxPx, focal, pxSize, ref);
    if (updateTrackLabel && m_view)
        m_view->setTrackDistance(QString::number(dist, 'f', 1) + QStringLiteral(" m (估算)"));
    Q_UNUSED(deviceId);
    return dist;
}

void PresenterMapService::updateAiInfo(const QString& deviceId, const QJsonObject& doc,
                                       const DeviceState& state)
{
    const DeviceConfig& camCfg = deviceCam(deviceId);
    const int pip = DeviceState::pipShowToComboIndex(state.currentPipShow);
    const bool isVis = pip != 1 && pip != 4;
    CameraIntrinsics camInfo;
    camInfo.pixelSizeUm = isVis ? camCfg.visPixelSize : camCfg.irPixelSize;
    camInfo.focalLengthMm = isVis ? camCfg.visMinFocal * state.currentVisZoom
                                  : camCfg.irMinFocal * state.currentIrZoom;
    camInfo.resX = isVis ? camCfg.visResX : camCfg.irResX;
    camInfo.resY = isVis ? camCfg.visResY : camCfg.irResY;

    DevicePose pose;
    pose.lat = GeoCalculator::parseCoord(state.latitudeRaw);
    pose.lon = GeoCalculator::parseCoord(state.longitudeRaw);
    pose.panDeg = state.currentPan;
    double tilt = state.currentTilt;
    if (m_cfg->softwarePtzCalibrationEnabled()) {
        pose.panDeg -= m_cfg->ptzPanOffset();
        while (pose.panDeg < 0) pose.panDeg += 360;
        while (pose.panDeg >= 360) pose.panDeg -= 360;
        tilt -= m_cfg->ptzTiltOffset();
        while (tilt < -180) tilt += 360;
        while (tilt > 180) tilt -= 360;
    }

    const QJsonObject objects = doc.value("Object").toObject();
    DeviceMapState& saved = mapState(deviceId);
    auto makeBbox = [&](const QJsonObject& pts, double dist) {
        QJsonArray bbox;
        double lat, lon;
        const int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
        const int r = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
        for (const QPoint& p : {QPoint(l, t), QPoint(r, t), QPoint(r, b), QPoint(l, b)}) {
            GeoCalculator::pixelBboxToGps(p.x(), p.y(), dist, tilt, camInfo, pose, lat, lon);
            bbox.append(QJsonArray{lat, lon});
        }
        return bbox;
    };

    const int workMode = state.aiWorkMode ? state.aiWorkMode : state.workMode;
    if (workMode == 1) {
        if (objects.isEmpty()) {
            m_view->mapClearAllTracks();
            m_view->mapClearFov();
            m_view->mapUpdateTargetMarkers(QJsonArray());
            return;
        }
        QJsonArray targets;
        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const QJsonObject obj = it.value().toObject();
            if (!obj.contains("Points")) continue;
            const int cls = obj.value("Class").toInt();
            const double dist = calculateVisualDistance(deviceId, obj, cls, state, false);
            const QJsonObject pts = obj.value("Points").toObject();
            const double cx = (pts.value("Left").toInt() + pts.value("Right").toInt()) / 2.0;
            const double cy = (pts.value("Top").toInt() + pts.value("Bottom").toInt()) / 2.0;
            double lat = 0, lon = 0;
            GeoCalculator::pixelToGps(cx, cy, dist, camInfo, pose, lat, lon);
            QJsonObject target{{"id", it.key()}, {"cls", cls}, {"dist", dist},
                               {"lat", lat}, {"lon", lon}, {"locked", false},
                               {"bbox", makeBbox(pts, dist)}};
            targets.append(target);
        }
        m_view->mapUpdateTargetMarkers(targets);
        return;
    }
    if (workMode < 2 || workMode > 4) return;

    QString lockedId, lostId;
    QJsonObject lockedObj, lostObj;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const QJsonObject obj = it.value().toObject();
        const int st = obj.value("State").toInt(); // State：0xB1 跟踪正常 / 0xB2 跟踪丢失
        if (st == 0xB1 && lockedId.isEmpty()) { lockedId = it.key(); lockedObj = obj; }
        else if (st == 0xB2 && lostId.isEmpty()) { lostId = it.key(); lostObj = obj; }
    }

    if (!lockedId.isEmpty()) {
        saved.track.lostSince = QDateTime();
        const int cls = lockedObj.value("Class").toInt();
        const double dist = calculateVisualDistance(deviceId, lockedObj, cls, state, true);
        saved.lastAiDist = dist;
        saved.lastAiDistEstimated = lockedObj.value("Distance").toDouble(0) <= 0 && dist > 0;
        if (!lockedObj.contains("Points")) return;
        const QJsonObject pts = lockedObj.value("Points").toObject();
        const double cx = (pts.value("Left").toInt() + pts.value("Right").toInt()) / 2.0;
        const double cy = (pts.value("Top").toInt() + pts.value("Bottom").toInt()) / 2.0;
        double lat = 0, lon = 0;
        GeoCalculator::pixelToGps(cx, cy, dist, camInfo, pose, lat, lon);
        double speed = 0;
        const QDateTime now = QDateTime::currentDateTime();
        if (saved.track.prevTime.isValid()) {
            const double dt = saved.track.prevTime.msecsTo(now) / 1000.0;
            if (dt > 0) speed = GeoCalculator::haversineDistance(saved.track.prevLat, saved.track.prevLon, lat, lon) / dt;
        }
        saved.track.prevLat = lat; saved.track.prevLon = lon; saved.track.prevTime = now;
        if (lat != 0 && lon != 0) {
            const bool changed = saved.track.id != lockedId;
            saved.track.id = lockedId;
            if (changed) saved.track.plotHeading = -1;
            double bearing = 0;
            if (GeoCalculator::shouldPlotTrackPoint(lat, lon, saved.track.plotLat, saved.track.plotLon,
                                                     saved.track.plotHeading, saved.track.plotTime, &bearing)) {
                m_view->mapAppendTrackPoint(lockedId, lat, lon, speed);
                saved.track.plotLat = lat; saved.track.plotLon = lon;
                saved.track.plotTime = now; saved.track.plotHeading = bearing;
            }
        }
        QJsonObject target{{"id", lockedId}, {"cls", cls}, {"dist", dist}, {"lat", lat}, {"lon", lon},
                           {"locked", true}, {"speed", speed}, {"bbox", makeBbox(pts, dist)}};
        m_view->mapUpdateTargetMarkers(QJsonArray{target});
        return;
    }
    if (!lostId.isEmpty()) {
        const int cls = lostObj.value("Class").toInt();
        const double dist = calculateVisualDistance(deviceId, lostObj, cls, state, true);
        if (saved.track.lostSince.isNull()) {
            saved.track.id = lostId; saved.track.cls = cls;
            saved.track.lostSince = QDateTime::currentDateTime();
            if (lostObj.contains("Points")) {
                const QJsonObject pts = lostObj.value("Points").toObject();
                const double cx = (pts.value("Left").toInt() + pts.value("Right").toInt()) / 2.0;
                const double cy = (pts.value("Top").toInt() + pts.value("Bottom").toInt()) / 2.0;
                GeoCalculator::pixelToGps(cx, cy, dist, camInfo, pose, saved.track.lat, saved.track.lon);
            }
        }
        if (saved.track.lostSince.msecsTo(QDateTime::currentDateTime()) >= 5000) {
            m_view->mapClearAllTracks(); m_view->mapUpdateTargetMarkers(QJsonArray()); return;
        }
        if (!lostObj.contains("Points"))
            return;
        QJsonObject target{{"id", saved.track.id}, {"cls", saved.track.cls}, {"dist", dist},
                           {"lat", saved.track.lat}, {"lon", saved.track.lon}, {"locked", false}, {"speed", 0}};
        m_view->mapUpdateTargetMarkers(QJsonArray{target});
        return;
    }
    m_view->mapClearAllTracks();
    m_view->mapUpdateTargetMarkers(QJsonArray());
}

void PresenterMapService::updateDevicePosition(const QString& deviceId, const DeviceState& state)
{
    const double lat = GeoCalculator::parseCoord(state.latitudeRaw);
    const double lon = GeoCalculator::parseCoord(state.longitudeRaw);
    if (lat == 0 && lon == 0) return;
    double pan = state.currentPan, tilt = state.currentTilt;
    if (m_cfg->softwarePtzCalibrationEnabled()) {
        pan -= m_cfg->ptzPanOffset();
        while (pan < 0) pan += 360; while (pan >= 360) pan -= 360;
        tilt -= m_cfg->ptzTiltOffset();
        while (tilt < -180) tilt += 360; while (tilt > 180) tilt -= 360;
    }
    DeviceMapState& saved = mapState(deviceId);
    double range = state.laserRange;
    bool estimated = false;
    if (range <= 0) { range = saved.lastAiDist; estimated = saved.lastAiDistEstimated; }
    const DeviceConfig& cam = deviceCam(deviceId);
    const double visW = cam.visPixelSize * cam.visResX / 1000.0;
    const double visFocal = cam.visMinFocal * state.currentVisZoom;
    const double hfov = 2 * qAtan(visW / (2 * visFocal)) * 180.0 / M_PI;
    const double vfov = hfov * cam.visResY / cam.visResX;
    m_view->mapSetDevicePosition(lat, lon);
    m_view->mapSetVisFov(lat, lon, pan, tilt, hfov, vfov, m_cfg->visFovDistance());
    const double irW = cam.irPixelSize * cam.irResX / 1000.0;
    const double irFocal = cam.irMinFocal * state.currentIrZoom;
    const double irHfov = 2 * qAtan(irW / (2 * irFocal)) * 180.0 / M_PI;
    m_view->mapSetIrFov(lat, lon, pan, tilt, irHfov, irHfov * cam.irResY / cam.irResX, m_cfg->irFovDistance());
    m_view->mapSetDeviceInfo(lat, lon, state.altitude, pan, tilt, hfov, vfov, range, estimated);
}
