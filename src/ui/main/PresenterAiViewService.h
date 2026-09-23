#ifndef PRESENTERAIVIEWSERVICE_H
#define PRESENTERAIVIEWSERVICE_H

#include <QObject>
#include <QJsonObject>

#include "core/DeviceState.h"
#include "core/DeviceConfig.h"
#include "PresenterStateViewService.h"

class IMainView;
class ConfigManager;
class PresenterMapService;

class PresenterAiViewService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterAiViewService(IMainView* view, ConfigManager* cfg,
                                    PresenterMapService* mapService,
                                    QObject* parent = nullptr);

    void updateAiInfo(const QString& deviceId, const QJsonObject& doc,
                      const DeviceState& state, const StateViewCache& cache);

private:
    const DeviceConfig& deviceCam(const QString& deviceId) const;

    IMainView* m_view;
    ConfigManager* m_cfg;
    PresenterMapService* m_mapService;
};

#endif // PRESENTERAIVIEWSERVICE_H
