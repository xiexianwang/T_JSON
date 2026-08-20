#include "PresenterMapService.h"
#include "ui/main/IMainView.h"
#include "infrastructure/configmanager.h"
#include "core/GeoCalculator.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>

PresenterMapService::PresenterMapService(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent), m_view(view), m_cfg(cfg)
{
}

void PresenterMapService::updateAiInfo(const QString& deviceId, const QJsonObject& doc)
{
    Q_UNUSED(deviceId);
    Q_UNUSED(doc);
}

void PresenterMapService::updateDevicePosition(const QString& deviceId, const DeviceState& state)
{
    Q_UNUSED(deviceId);
    Q_UNUSED(state);
}
