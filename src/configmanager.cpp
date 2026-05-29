// ============================================================
// 文件: configmanager.cpp
// 描述: ConfigManager 类的实现。通过 Qt 的 QSettings 机制将
//       配置持久化到注册表或本地文件，并在保存时通知关心配置
//       变更的模块。
// ============================================================

#include "configmanager.h"

// 构造函数：构造时即从本地存储加载全部配置
ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    load();
}

// 从 QSettings（组织 "Tofu"，应用 "T-JSON_Settings"）加载所有配置
void ConfigManager::load()
{
    QSettings settings("Tofu", "T-JSON_Settings");
    m_ptz.load(settings);     // 加载云台参数
    m_lens.load(settings);    // 加载镜头参数
    m_cam.load(settings);     // 加载相机参数
    m_serialIp   = settings.value("SerialIp", "192.168.1.66").toString();
    m_serialPort = static_cast<quint16>(settings.value("SerialPort", 4001).toUInt());
}

// 重新加载：直接委托给 load() 以实现刷新
void ConfigManager::reload()
{
    load();
}

// 保存所有配置到本地存储，并发射变更信号通知其他模块
void ConfigManager::save()
{
    QSettings settings("Tofu", "T-JSON_Settings");
    m_ptz.save(settings);
    m_lens.save(settings);
    m_cam.save(settings);
    settings.setValue("SerialIp",   m_serialIp);
    settings.setValue("SerialPort", m_serialPort);
    emit ptzConfigChanged();     // 通知云台配置已更新
    emit lensConfigChanged();    // 通知镜头配置已更新
    emit cameraConfigChanged();  // 通知相机参数已更新
}
