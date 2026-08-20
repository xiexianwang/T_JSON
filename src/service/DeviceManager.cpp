#include "DeviceManager.h"
#include <QMutex>
#include <QDebug>



DeviceManager& DeviceManager::instance()
{
    static DeviceManager inst;
    return inst;
}

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
}

DeviceManager::~DeviceManager()
{
    // MainWindow 析构已调用 removeAllDevices()，此处仅兜底清空。
    // 不调用 shutdown() —— 静态析构阶段 QApplication 可能已不存在。
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
