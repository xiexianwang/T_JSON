#ifndef PRESENTERMAPSERVICE_H
#define PRESENTERMAPSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "core/DeviceState.h"

class IMainView;
class ConfigManager;

class PresenterMapService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterMapService(IMainView* view, ConfigManager* cfg, QObject *parent = nullptr);

    void updateAiInfo(const QString& deviceId, const QJsonObject& doc);
    void updateDevicePosition(const QString& deviceId, const DeviceState& state);

private:
    IMainView* m_view;
    ConfigManager* m_cfg;
};

#endif // PRESENTERMAPSERVICE_H
