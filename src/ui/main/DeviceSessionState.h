#ifndef DEVICESESSIONSTATE_H
#define DEVICESESSIONSTATE_H

#include <QHash>
#include <QString>
#include "core/DeviceTypes.h"

class DeviceSessionState
{
public:
    bool ensure(const DeviceId& deviceId);
    bool select(const DeviceId& deviceId);
    bool remove(const DeviceId& deviceId);

    DeviceId selectedDeviceId() const { return m_selectedDeviceId; }
    quint64 generation(const DeviceId& deviceId) const;
    bool contains(const DeviceId& deviceId) const { return m_active.contains(deviceId); }
    bool accepts(const DeviceId& deviceId, quint64 generation) const;

private:
    QHash<DeviceId, quint64> m_active;
    QHash<DeviceId, quint64> m_nextGeneration;
    DeviceId m_selectedDeviceId;
};

#endif // DEVICESESSIONSTATE_H
