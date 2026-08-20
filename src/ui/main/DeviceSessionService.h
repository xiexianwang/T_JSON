#ifndef DEVICESESSIONSERVICE_H
#define DEVICESESSIONSERVICE_H

#include <QObject>
#include <QHash>
#include "DeviceSessionState.h"

class ConfigManager;
class DeviceContext;

class DeviceSessionService : public QObject
{
    Q_OBJECT
public:
    explicit DeviceSessionService(ConfigManager* cfg, QObject* parent = nullptr);

    DeviceContext* ensureDevice(const DeviceId& deviceId);
    bool selectDevice(const DeviceId& deviceId);
    bool removeDevice(const DeviceId& deviceId);

    DeviceContext* currentDevice() const;
    DeviceId currentDeviceId() const { return m_state.selectedDeviceId(); }
    quint64 generation(const DeviceId& deviceId) const { return m_state.generation(deviceId); }
    bool accepts(const DeviceId& deviceId, quint64 generation) const
    {
        return m_state.accepts(deviceId, generation);
    }

signals:
    void deviceSelected(const DeviceId& previousId, const DeviceId& currentId);
    void deviceRemoved(const DeviceId& deviceId, quint64 generation);

private:
    ConfigManager* m_cfg;
    DeviceSessionState m_state;
};

#endif // DEVICESESSIONSERVICE_H
