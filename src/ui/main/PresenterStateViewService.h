#ifndef PRESENTERSTATEVIEWSERVICE_H
#define PRESENTERSTATEVIEWSERVICE_H

#include <QObject>
#include <QString>
#include "core/DeviceState.h"
#include "core/DeviceConfig.h"

class IMainView;
class ConfigManager;
class PresenterMapService;

struct StateViewCache
{
    double currentVisZoom = 1.0;
    double currentIrZoom = 1.0;
    double currentTilt = 0.0;
    int currentPipShow = 0;
    int previousWorkMode = 0;
    bool workModeInitialized = false;
    bool displayModeInitialized = false;
    bool algoModelInitialized = false;
    int previousAlgoModel = 0;
    int currentAlgoModel = 0;
    int previousDisplayMode = 0;
    int currentResX = 2688;
    int currentResY = 1520;
};

class PresenterStateViewService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterStateViewService(IMainView* view, ConfigManager* cfg,
                                       PresenterMapService* mapService,
                                       QObject* parent = nullptr);

    StateViewCache updateStatusFromState(const QString& deviceId,
                                         const DeviceState& state,
                                         const StateViewCache& previous);

private:
    void updateLensStats(const StateViewCache& cache, bool hasZoomInfo,
                         const DeviceConfig& cam);
    const DeviceConfig& deviceCam(const QString& deviceId) const;

    IMainView* m_view;
    ConfigManager* m_cfg;
    PresenterMapService* m_mapService;
};

#endif // PRESENTERSTATEVIEWSERVICE_H
