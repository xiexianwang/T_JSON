#include "DeviceSessionState.h"

bool DeviceSessionState::ensure(const DeviceId& deviceId)
{
    if (deviceId.isEmpty() || m_active.contains(deviceId)) return false;
    const quint64 next = m_nextGeneration.value(deviceId, 0) + 1;
    m_nextGeneration.insert(deviceId, next);
    m_active.insert(deviceId, next);
    if (m_selectedDeviceId.isEmpty()) m_selectedDeviceId = deviceId;
    return true;
}

bool DeviceSessionState::select(const DeviceId& deviceId)
{
    if (!m_active.contains(deviceId) || m_selectedDeviceId == deviceId) return false;
    m_selectedDeviceId = deviceId;
    return true;
}

bool DeviceSessionState::remove(const DeviceId& deviceId)
{
    if (!m_active.remove(deviceId)) return false;
    m_nextGeneration.insert(deviceId, m_nextGeneration.value(deviceId) + 1);
    if (m_selectedDeviceId == deviceId) {
        m_selectedDeviceId = m_active.isEmpty() ? DeviceId() : m_active.constBegin().key();
    }
    return true;
}

quint64 DeviceSessionState::generation(const DeviceId& deviceId) const
{
    return m_active.value(deviceId, 0);
}

bool DeviceSessionState::accepts(const DeviceId& deviceId, quint64 generation) const
{
    return generation != 0 && m_active.value(deviceId, 0) == generation;
}
