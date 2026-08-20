#include "DeviceManager.h"
#include <QMutex>
#include <QDebug>



DeviceManager* DeviceManager::instance()
{
    static DeviceManager* instance = new DeviceManager(); return instance;
}

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
}

DeviceManager::~DeviceManager()
{
    // 不在此调用 removeAllDevices() —— MainWindow 析构已负责清理。
    // 此处仅兜底：若仍有残留设备，逐个关闭但不显式 delete
    // （QObject 析构会自动清理子对象）。
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        it.value()->shutdown();
    }
    m_devices.clear();
}

void DeviceManager::init(ConfigManager* cfg)
{
    m_globalCfg = cfg;
}

DeviceContext* DeviceManager::addDevice(const QString& deviceId)
{
    if (m_devices.contains(deviceId)) {
        return m_devices.value(deviceId);
    }
    
    DeviceContext* ctx = new DeviceContext(deviceId, m_globalCfg, this);
    m_devices.insert(deviceId, ctx);
    return ctx;
}

void DeviceManager::removeDevice(const QString& deviceId)
{
    if (m_devices.contains(deviceId)) {
        DeviceContext* ctx = m_devices.value(deviceId);
        const DeviceContext::ShutdownResult result = ctx->shutdown();
        if (!result.succeeded()) {
            qWarning() << "DeviceManager: device shutdown incomplete, keeping context:" << deviceId
                       << result.error;
            return;
        }
        m_devices.remove(deviceId);
        ctx->setParent(nullptr);
        delete ctx;
    }
}

DeviceContext* DeviceManager::getDevice(const QString& deviceId) const
{
    return m_devices.value(deviceId, nullptr);
}

QList<QString> DeviceManager::getAllDeviceIds() const
{
    return m_devices.keys();
}

void DeviceManager::removeAllDevices()
{
    const QList<QString> deviceIds = m_devices.keys();
    for (const QString& deviceId : deviceIds) {
        removeDevice(deviceId);
    }
}
