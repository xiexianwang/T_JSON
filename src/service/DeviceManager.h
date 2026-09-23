#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include "DeviceContext.h"
#include "infrastructure/configmanager.h"

// ============================================================================
// DeviceManager - 多设备管理器 (单例)
// 负责所有 DeviceContext 的生命周期管理（增、删、改、查）
// ============================================================================
class DeviceManager : public QObject
{
    Q_OBJECT
public:
    static DeviceManager& instance();

    // 禁用拷贝与赋值
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    // 初始化管理器，需要传入配置指针
    void init(ConfigManager* cfg);

    // 设备生命周期管理
    DeviceContext* addDevice(const QString& deviceId, const DeviceConfig& devCfg = DeviceConfig());
    void removeDevice(const QString& deviceId);
    DeviceContext* getDevice(const QString& deviceId) const;
    
    // 获取当前所有设备 ID 列表
    QList<QString> getAllDeviceIds() const;

    // 断开并清理所有设备
    void removeAllDevices();

private:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    ConfigManager* m_globalCfg = nullptr;
    QMap<QString, DeviceContext*> m_devices;
};

#endif // DEVICEMANAGER_H
