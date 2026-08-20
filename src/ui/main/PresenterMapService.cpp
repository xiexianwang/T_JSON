#include "PresenterMapService.h"
#include "MainPresenter.h"
#include "ui/main/IMainView.h"
#include "infrastructure/configmanager.h"
#include "core/GeoCalculator.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>

PresenterMapService::PresenterMapService(MainPresenter* parentPresenter, IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent), m_presenter(parentPresenter), m_view(view), m_cfg(cfg)
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
