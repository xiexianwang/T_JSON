#include "PresenterStateViewService.h"
#include "IMainView.h"
#include "PresenterMapService.h"
#include "infrastructure/configmanager.h"
#include "core/DeviceState.h"

#include <QtMath>

PresenterStateViewService::PresenterStateViewService(IMainView* view, ConfigManager* cfg,
                                                     PresenterMapService* mapService,
                                                     QObject* parent)
    : QObject(parent), m_view(view), m_cfg(cfg), m_mapService(mapService)
{
}

StateViewCache PresenterStateViewService::updateStatusFromState(const QString& deviceId,
                                                                 const DeviceState& state,
                                                                 const StateViewCache& previous)
{
    StateViewCache cache = previous;
    CameraConfig& cam = m_cfg->cam();
    cache.currentVisZoom = state.currentVisZoom;
    cache.currentIrZoom = state.currentIrZoom;

    QString heightStr;
    if (state.altitude != 0.0)
        heightStr = QString::number(state.altitude, 'f', 1) + QStringLiteral(" m");
    double rawPan = state.currentPan;
    double rawTilt = state.currentTilt;
    if (m_cfg->softwarePtzCalibrationEnabled()) {
        rawPan -= m_cfg->ptzPanOffset();
        while (rawPan < 0.0) rawPan += 360.0;
        while (rawPan >= 360.0) rawPan -= 360.0;
        rawTilt -= m_cfg->ptzTiltOffset();
        while (rawTilt < -180.0) rawTilt += 360.0;
        while (rawTilt > 180.0) rawTilt -= 360.0;
    }
    cache.currentTilt = rawTilt;
    m_view->showDeviceState(state.camShowMode, state.latitudeRaw, state.longitudeRaw,
                            heightStr,
                            QString::number(rawPan, 'f', 1) + QStringLiteral("°"),
                            QString::number(rawTilt, 'f', 1) + QStringLiteral("°"));
    updateLensStats(cache);
    m_mapService->updateDevicePosition(deviceId, state);

    static const char* resMap[] = {"1080P", "720P", "D1", "1440P"};
    const QString resStr = (state.imgSize >= 0 && state.imgSize < 4)
        ? QString::fromLatin1(resMap[state.imgSize]) : QString::number(state.imgSize);
    cache.currentResX = state.resX;
    cache.currentResY = state.resY;
    const QString bitrateStr = QString("%1 Kb/s").arg(state.bitrate);
    static const char* codecMap[] = {"H264", "H265"};
    const QString codecStr = (state.codec >= 0 && state.codec < 2)
        ? QString::fromLatin1(codecMap[state.codec]) : QString::number(state.codec);
    static const char* wmMap[] = {"关闭AI", "识别", "自动跟踪", "点选跟踪", "波门/框选跟踪"};
    const int wm = state.workMode;
    const QString wmStr = (wm >= 0 && wm < 5) ? QString::fromUtf8(wmMap[wm]) : QString::number(wm);
    cache.previousWorkMode = wm;

    static const char* pipMap[] = {"大图可见光", "红外", "可见光", "融合", "大图红外"};
    const int pipRaw = state.currentPipShow;
    const int comboIdx = DeviceState::pipShowToComboIndex(pipRaw);
    const QString pipStr = (comboIdx >= 0 && comboIdx < 5)
        ? QString::fromUtf8(pipMap[comboIdx]) : QString::number(pipRaw);
    const int model = state.model;
    const int high = model / 10;
    const int low = model % 10;
    static const char* highMap[] = {"可见光", "红外"};
    static const char* lowMap[] = {"", "", "人车识别", "船识别", "无人机识别", "飞机直升机识别", "鸟识别"};
    QString modelStr;
    if (high >= 0 && high < 2) modelStr = QString::fromUtf8(highMap[high]);
    if (low >= 2 && low <= 6) modelStr += QString(" / %1").arg(QString::fromUtf8(lowMap[low]));
    if (modelStr.isEmpty()) modelStr = QString::number(model);
    cache.previousAlgoModel = model;
    m_view->showImageParams(resStr, bitrateStr, codecStr, wmStr, pipStr, modelStr,
                            state.maxVisFL, state.maxIRFL);
    cache.currentPipShow = comboIdx;
    cache.previousDisplayMode = comboIdx;

    if (!cache.algoModelInitialized) {
        cache.currentAlgoModel = model;
        m_view->setAlgoModel1Index(high);
        if (low >= 2 && low <= 6) m_view->setAlgoModel2Index(low - 2);
        cache.algoModelInitialized = true;
    }
    if (!cache.displayModeInitialized) {
        m_view->setDisplayModeIndex(comboIdx);
        cache.displayModeInitialized = true;
    }
    if (!cache.workModeInitialized && wm >= 0) {
        m_view->setWorkModeIndex(wm);
        cache.workModeInitialized = true;
    }
    return cache;
}

void PresenterStateViewService::updateLensStats(const StateViewCache& cache)
{
    const CameraConfig& cam = m_cfg->cam();
    const double kRad2Deg = 180.0 / 3.14159265358979323846;
    const double visFocal = cam.visMinFocal * cache.currentVisZoom;
    const double irFocal = cam.irMinFocal * cache.currentIrZoom;
    const double visHfov = 2.0 * qAtan((cam.visPixelSize * cam.visResX / 1000.0) / (2.0 * visFocal));
    const double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));
    m_view->showLensStats(cache.currentVisZoom, visFocal, visHfov * kRad2Deg,
                          cache.currentIrZoom, irFocal, irHfov * kRad2Deg);
}
