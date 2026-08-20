#include "DeviceSessionService.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"

DeviceSessionService::DeviceSessionService(ConfigManager* cfg, QObject* parent)
    : QObject(parent)
    , m_cfg(cfg)
{
    DeviceManager::instance()->init(m_cfg);
}

DeviceContext* DeviceSessionService::ensureDevice(const DeviceId& deviceId)
{
    if (deviceId.isEmpty()) return nullptr;
    DeviceManager* manager = DeviceManager::instance();
    DeviceContext* context = manager->getDevice(deviceId);
    if (!context) {
        m_state.ensure(deviceId);
        context = manager->addDevice(deviceId);
    } else if (!m_state.contains(deviceId)) {
        m_state.ensure(deviceId);
    }
    return context;
}

bool DeviceSessionService::selectDevice(const DeviceId& deviceId)
{
    const DeviceId previous = m_state.selectedDeviceId();
    if (!m_state.select(deviceId)) return false;
    emit deviceSelected(previous, deviceId);
    return true;
}

bool DeviceSessionService::removeDevice(const DeviceId& deviceId)
{
    const quint64 oldGeneration = m_state.generation(deviceId);
    if (!m_state.contains(deviceId)) return false;
    DeviceManager::instance()->removeDevice(deviceId);
    if (DeviceManager::instance()->getDevice(deviceId)) return false;
    if (!m_state.remove(deviceId)) return false;
    emit deviceRemoved(deviceId, oldGeneration);
    return true;
}

DeviceContext* DeviceSessionService::currentDevice() const
{
    return DeviceManager::instance()->getDevice(m_state.selectedDeviceId());
}
