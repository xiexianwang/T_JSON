#include "trackmanager.h"
#include "geoutils.h"
#include "jsonframeparser.h"
#include "configmanager.h"

TrackManager::TrackManager(QObject *parent)
    : QObject(parent)
{
}

void TrackManager::reset()
{
    m_track = TrackState();
    m_lastAiDist = 0;
    m_lastAiDistEstimated = false;
}

double TrackManager::calcVisualDistance(const QString& rawDist, int cls,
                                         int boxPx, int algoModel,
                                         double visZoom, double irZoom, int pipShow,
                                         const CameraConfig& cam,
                                         bool* estimated)
{
    double dist = rawDist.toDouble();
    if (dist > 0 || boxPx <= 0) {
        if (estimated) *estimated = false;
        return dist;
    }

    int low = algoModel % 10;
    double ref = cam.targetRefSize(low, cls);

    if (ref <= 0 && (cls == 0xB1 || cls == 0xB2)) {
        static const int fallback[] = {0xA1, 0xA2, 0xA3, 0xA4};
        for (int fc : fallback) {
            ref = cam.targetRefSize(low, fc);
            if (ref > 0) break;
        }
    }

    if (ref <= 0) return dist;

    bool isVis = (pipShow != 1 && pipShow != 4);
    double pxSize = isVis ? cam.visPixelSize : cam.irPixelSize;
    double focal = isVis ? cam.visMinFocal * visZoom : cam.irMinFocal * irZoom;

    dist = GeoUtils::estimateTargetDistance(boxPx, focal, pxSize, ref);
    if (estimated) *estimated = true;
    return dist;
}

TrackManager::UpdateResult TrackManager::processAiFrame(
    const AiInfoData& ai, int workMode,
    double devLat, double devLon, double pan, double tilt,
    double pixelSize, double focal,
    int halfW, int halfH,
    const CameraConfig& cam, int algoModel,
    double visZoom, double irZoom, int pipShow)
{
    UpdateResult result;

    if (workMode >= 2 && workMode <= 4) {
        // Track mode: find locked (0xB1) or lost (0xB2)
        int lockedIdx = -1, lostIdx = -1;
        for (int i = 0; i < ai.targets.size(); ++i) {
            if (ai.targets[i].cls == 0xB1 && lockedIdx < 0)
                lockedIdx = i;
            else if (ai.targets[i].cls == 0xB2 && lostIdx < 0)
                lostIdx = i;
        }

        // Locked target
        if (lockedIdx >= 0) {
            const auto& t = ai.targets[lockedIdx];
            m_track.lostSince = QDateTime();

            result.hasLock = true;
            result.id = t.id;
            result.cls = t.cls;

            if (t.hasPoints) {
                double cx = (t.left + t.right) / 2.0;
                double cy = (t.top + t.bottom) / 2.0;
                GeoUtils::pixelToGps(cx, cy, t.distance, devLat, devLon, pan,
                                     pixelSize, focal, halfW, halfH,
                                     result.lat, result.lon);

                m_track.lat = result.lat;
                m_track.lon = result.lon;
                m_track.cls = t.cls;

                // Speed calculation
                double speed = 0;
                if (m_track.prevTime.isValid()) {
                    double dist_m = GeoUtils::haversineDistance(
                        m_track.prevLat, m_track.prevLon, result.lat, result.lon);
                    double dt_s = m_track.prevTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
                    if (dt_s > 0) speed = dist_m / dt_s;
                }
                m_track.prevLat = result.lat;
                m_track.prevLon = result.lon;
                m_track.prevTime = QDateTime::currentDateTime();
                result.speed = speed;

                // Visual distance
                int boxPx = qMax(t.right - t.left, t.bottom - t.top);
                bool estimated = false;
                double dist = calcVisualDistance(
                    QString::number(t.distance, 'f', 0), t.cls, boxPx,
                    algoModel, visZoom, irZoom, pipShow, cam, &estimated);
                result.dist = dist;
                m_lastAiDist = dist;
                m_lastAiDistEstimated = estimated;

                // Track thinning
                result.targetChanged = (m_track.id != t.id);
                m_track.id = t.id;
                if (result.targetChanged)
                    m_track.plotHeading = -1;

                double outBearing = 0;
                if (result.lat != 0 && result.lon != 0 &&
                    GeoUtils::shouldPlotTrackPoint(
                        result.lat, result.lon,
                        m_track.plotLat, m_track.plotLon,
                        m_track.plotHeading, m_track.plotTime, &outBearing)) {
                    result.shouldPlot = true;
                    result.plotBearing = outBearing;
                    m_track.plotLat = result.lat;
                    m_track.plotLon = result.lon;
                    m_track.plotTime = QDateTime::currentDateTime();
                    m_track.plotHeading = outBearing;
                }
            }

            emit stateChanged();
            return result;
        }

        // Lost target
        if (lostIdx >= 0) {
            const auto& t = ai.targets[lostIdx];
            result.hasLost = true;
            result.cls = t.cls;

            if (t.hasPoints) {
                double cx = (t.left + t.right) / 2.0;
                double cy = (t.top + t.bottom) / 2.0;
                GeoUtils::pixelToGps(cx, cy, t.distance, devLat, devLon, pan,
                                     pixelSize, focal, halfW, halfH,
                                     result.lat, result.lon);
            }

            if (m_track.lostSince.isNull()) {
                m_track.id = t.id;
                m_track.lat = result.lat;
                m_track.lon = result.lon;
                m_track.cls = t.cls;
                m_track.lostSince = QDateTime::currentDateTime();
            }

            qint64 elapsed = m_track.lostSince.msecsTo(QDateTime::currentDateTime());
            if (elapsed >= 5000) {
                result.cleared = true;
                emit stateChanged();
                return result;
            }

            // Visual distance
            bool estimated = false;
            int boxPx = t.hasPoints ? qMax(t.right - t.left, t.bottom - t.top) : 0;
            result.dist = calcVisualDistance(
                QString::number(t.distance, 'f', 0), t.cls, boxPx,
                algoModel, visZoom, irZoom, pipShow, cam, &estimated);
            m_lastAiDist = result.dist;
            m_lastAiDistEstimated = estimated;

            emit stateChanged();
            return result;
        }

        // No 0xB1/0xB2 -> clear
        result.cleared = true;
    } else if (workMode == 1) {
        // Identify mode: nothing to track
        return result;
    }

    emit stateChanged();
    return result;
}
