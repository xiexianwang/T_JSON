#include "PresenterAiViewService.h"

#include "IMainView.h"
#include "PresenterMapService.h"
#include "infrastructure/configmanager.h"
#include "core/AiTargetType.h"
#include "core/GeoCalculator.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"

#include <QtMath>

PresenterAiViewService::PresenterAiViewService(IMainView* view, ConfigManager* cfg,
                                               PresenterMapService* mapService,
                                               QObject* parent)
    : QObject(parent), m_view(view), m_cfg(cfg), m_mapService(mapService)
{
}

// 获取指定设备的相机/云台配置；无上下文时回退到默认值
const DeviceConfig& PresenterAiViewService::deviceCam(const QString& deviceId) const
{
    static const DeviceConfig kDefaultCfg;
    auto* ctx = DeviceManager::instance().getDevice(deviceId);
    return ctx ? ctx->deviceConfig() : kDefaultCfg;
}

void PresenterAiViewService::updateAiInfo(const QString& deviceId, const QJsonObject& doc,
                                          const DeviceState& inputState,
                                          const StateViewCache& cache)
{
    const DeviceConfig& cam = deviceCam(deviceId);
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
            const QString typeName = AiTargetType::name(inputState.model, cls);
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
            m_view->addIdentifyRow(id, typeName, dist, pos, miss);
        }
    }

    if (workMode >= 1 && workMode <= 4) {
        DeviceState state = inputState;
        state.aiWorkMode = workMode;
        m_mapService->updateAiInfo(deviceId, doc, state);
    }

    if (workMode < 2 || workMode > 4) return;

    // 跟踪模式：设备仅推送被跟踪目标。目标筛选优先 State==0xB1（跟踪正常），
    // 其次 State==0xB2（跟踪丢失）；缺 State 字段时（旧固件）兜底首元素并按锁定处理。
    // 注意：Class 是目标类型代码，不能用来判定跟踪状态。
    QJsonObject obj;
    bool tracked = false;
    bool locked = false;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        const int st = o.value("State").toInt();
        if (st == 0xB1) { obj = o; tracked = true; locked = true; break; }
        if (st == 0xB2 && !tracked) { obj = o; tracked = true; locked = false; }
    }
    if (!tracked && !objects.isEmpty()) {
        obj = objects.begin().value().toObject();
        tracked = true;
        locked = true;
    }

    if (tracked) {
        const int cls = obj.value("Class").toInt();

        m_view->showTrackStatus(locked ? QString::fromUtf8("状态: 跟踪正常")
                                       : QString::fromUtf8("状态: 跟踪丢失"),
                                locked ? "locked" : "missed");
        m_view->setTrackTargetType(AiTargetType::name(inputState.model, cls));

        const double rawDist = obj.value("Distance").toDouble(0);
        if (rawDist > 0) {
            m_view->setTrackDistance(QString::number(rawDist, 'f', 1) + QStringLiteral(" m"));
        } else {
            // 无激光/设备未上报 Distance：回退视觉测距估算（依赖 targetRefMap 参考尺寸）
            const double est = m_mapService->calculateVisualDistance(deviceId, obj, cls,
                                                                      inputState, false);
            if (est > 0)
                m_view->setTrackDistance(QString::number(est, 'f', 1) + QStringLiteral(" m (估算)"));
            else
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

            // 角度：优先设备上报 Angle；旧固件缺字段时用像素偏移换算（rad→deg）兜底
            if (obj.contains("Angle") && obj.value("Angle").isObject()) {
                const QJsonObject ang = obj.value("Angle").toObject();
                m_view->setTrackAngle(QString("H: %1°  V: %2°")
                    .arg(ang.value("Hor").toDouble(0), 0, 'f', 1)
                    .arg(ang.value("Ver").toDouble(0), 0, 'f', 1));
            } else if (fl > 0) {
                m_view->setTrackAngle(QString("H: %1°  V: %2°")
                    .arg(dx * px / fl * 180.0 / M_PI, 0, 'f', 2)
                    .arg(dy * px / fl * 180.0 / M_PI, 0, 'f', 2));
            } else {
                m_view->setTrackAngle(QString());
            }
        } else {
            m_view->setTrackPos(QString());
            m_view->setTrackMissDistance(QString());
            m_view->setTrackAngle(QString());
        }
    } else {
        m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
        m_view->setTrackPos(QString());
        m_view->setTrackMissDistance(QString());
        m_view->setTrackDistance(QString());
        m_view->setTrackTargetType(QString());
        m_view->setTrackAngle(QString());
    }
}
