#ifndef DEVICESTATESERVICE_H
#define DEVICESTATESERVICE_H

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <memory>

#include "core/DeviceState.h"
#include "core/DeviceTypes.h"

class DeviceStateService : public QObject
{
    Q_OBJECT

public:
    explicit DeviceStateService(QObject* parent = nullptr);

    void updateState(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void updateAi(const QString& deviceId, const QJsonObject& aiInfo);
    DeviceSnapshot snapshot(const QString& deviceId) const;
    void deviceRemoved(const QString& deviceId);
    void clear();

signals:
    void stateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void aiUpdated(const QString& deviceId, const QJsonObject& aiInfo);
    void snapshotRemoved(const QString& deviceId);
    void cleared();

private:
    QHash<QString, DeviceSnapshot> m_snapshots;
};

#endif // DEVICESTATESERVICE_H
