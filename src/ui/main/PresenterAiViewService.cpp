#include "PresenterAiViewService.h"

#include "IMainView.h"
#include "PresenterMapService.h"
#include "infrastructure/configmanager.h"
#include "core/GeoCalculator.h"

PresenterAiViewService::PresenterAiViewService(IMainView* view, ConfigManager* cfg,
                                               PresenterMapService* mapService,
                                               QObject* parent)
    : QObject(parent), m_view(view), m_cfg(cfg), m_mapService(mapService)
{
}

void PresenterAiViewService::updateAiInfo(const QString& deviceId, const QJsonObject& doc,
                                          const DeviceState& inputState,
                                          const StateViewCache& cache)
{
    CameraConfig& cam = m_cfg->cam();
    const int workMode = doc.value("WorkMode").toInt();
    const int count = doc.value("ObjectCount").toInt();
    const QJsonObject objects = doc.value("Object").toObject();

    if (workMode == 1) {
        m_view->setIdentifyCount(QString::fromUtf8("目标总数: %1").arg(count));
        m_view->clearIdentifyTable();

        const bool isVis = (cache.currentPipShow != 1 && cache.currentPipShow != 4);
        const double px = isVis ? cam.visPixelSize : cam.irPixelSize;
        const double fl = isVis ? cam.visMinFocal * cache.currentVisZoom
                                : cam.irMinFocal * cache.currentIrZoom;
        const int halfW = (isVis ? cache.currentResX : cam.irResX) / 2;
        const int halfH = (isVis ? cache.currentResY : cam.irResY) / 2;

        for (auto it = objects.begin(); it != objects.end(); ++it) {
            const QString id = it.key();
            const QJsonObject obj = it.value().toObject();
            const int cls = obj.value("Class").toInt();
            const double dist = m_mapService->calculateVisualDistance(deviceId, obj, cls,
                                                                        inputState, false);
            QString pos, miss;
            if (obj.contains("Points")) {
                const QJsonObject pts = obj.value("Points").toObject();
                const int l = pts.value("Left").toInt();
                const int t = pts.value("Top").toInt();
                const int r = pts.value("Right").toInt();
                const int b = pts.value("Bottom").toInt();
                pos = QString("(%1,%2)").arg(l).arg(t);
                const double cx = (l + r) / 2.0;
                const double cy = (t + b) / 2.0;
                miss = GeoCalculator::missMradStr(cx - halfW, cy - halfH, px, fl);
            }
            m_view->addIdentifyRow(id, cls, dist, pos, miss);
        }
    }

    if (workMode >= 1 && workMode <= 4) {
        DeviceState state = inputState;
        state.aiWorkMode = workMode;
        m_mapService->updateAiInfo(deviceId, doc, state);
    }

    if (workMode < 2 || workMode > 4) return;

    if (!objects.isEmpty()) {
        const QJsonObject obj = objects.begin().value().toObject();
        const int cls = obj.value("Class").toInt();
        const bool locked = (cls == 0xB1);
        const QString statusText = locked ? QString::fromUtf8("锁定中") : QString::fromUtf8("丢失");
        m_view->showTrackStatus(QString::fromUtf8("状态: %1").arg(statusText),
                                locked ? "locked" : "missed");

        if (obj.contains("Distance")) {
            const double rawDist = obj.value("Distance").toDouble(0);
            if (rawDist > 0)
                m_view->setTrackDistance(QString::number(rawDist, 'f', 1) + QStringLiteral(" m"));
        } else {
            m_view->setTrackDistance(QString());
        }

        if (obj.contains("Points")) {
            const QJsonObject pts = obj.value("Points").toObject();
            const int l = pts.value("Left").toInt();
            const int t = pts.value("Top").toInt();
            const int r = pts.value("Right").toInt();
            const int b = pts.value("Bottom").toInt();
            const int cx = (l + r) / 2;
            const int cy = (t + b) / 2;
            m_view->setTrackPos(QString("(%1,%2) %3×%4").arg(cx).arg(cy).arg(r - l).arg(b - t));

            const bool isVis = (cache.currentPipShow != 1 && cache.currentPipShow != 4);
            const double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            const double fl = isVis ? cam.visMinFocal * cache.currentVisZoom
                                    : cam.irMinFocal * cache.currentIrZoom;
            const double dx = (l + r) / 2.0 - (isVis ? cache.currentResX : cam.irResX) / 2;
            const double dy = (t + b) / 2.0 - (isVis ? cache.currentResY : cam.irResY) / 2;
            m_view->setTrackMissDistance(QString("H: %1  V: %2 mrad")
                .arg(dx * px / fl, 0, 'f', 2).arg(dy * px / fl, 0, 'f', 2));
        } else {
            m_view->setTrackPos(QString());
            m_view->setTrackMissDistance(QString());
        }
    } else {
        m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
        m_view->setTrackPos(QString());
        m_view->setTrackMissDistance(QString());
        m_view->setTrackDistance(QString());
    }
}
