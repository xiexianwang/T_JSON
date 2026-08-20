#include "DeviceStateService.h"

DeviceStateService::DeviceStateService(QObject* parent)
    : QObject(parent)
{
}

void DeviceStateService::updateState(const QString& deviceId, std::shared_ptr<DeviceState> state)
{
    if (deviceId.isEmpty() || !state) return;

    DeviceSnapshot& snapshot = m_snapshots[deviceId];
    snapshot.deviceId = deviceId;
    snapshot.exists = true;
    snapshot.hasState = true;
    snapshot.statePtr = std::make_shared<DeviceState>(*state);
    emit stateUpdated(deviceId, snapshot.statePtr);
}

void DeviceStateService::updateAi(const QString& deviceId, const QJsonObject& aiInfo)
{
    if (deviceId.isEmpty()) return;

    DeviceSnapshot& snapshot = m_snapshots[deviceId];
    snapshot.deviceId = deviceId;
    snapshot.exists = true;
    snapshot.hasAi = true;
    snapshot.aiInfo = aiInfo;
    emit aiUpdated(deviceId, snapshot.aiInfo);
}

DeviceSnapshot DeviceStateService::snapshot(const QString& deviceId) const
{
    const auto it = m_snapshots.constFind(deviceId);
    if (it == m_snapshots.constEnd()) return {};

    DeviceSnapshot result = it.value();
    if (result.statePtr) result.statePtr = std::make_shared<DeviceState>(*result.statePtr);
    return result;
}

void DeviceStateService::deviceRemoved(const QString& deviceId)
{
    if (m_snapshots.remove(deviceId)) emit snapshotRemoved(deviceId);
}

void DeviceStateService::clear()
{
    if (m_snapshots.isEmpty()) return;
    m_snapshots.clear();
    emit cleared();
}
