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

// 从 QSettings（组织 "LSS"，应用 "LSS_Video_Manager"）加载所有配置
void ConfigManager::load()
{
    QSettings settings("LSS", "LSS_Video_Manager");
    m_serialIp   = settings.value("SerialIp", "192.168.1.66").toString();
    m_serialPort = static_cast<quint16>(settings.value("SerialPort", 4001).toUInt());
    m_mockServerPort = static_cast<quint16>(settings.value("MockServerPort", 5001).toUInt());
    m_ptzPanOffset = settings.value("PtzPanOffset", 0.0).toDouble();
    m_ptzTiltOffset = settings.value("PtzTiltOffset", 0.0).toDouble();
    m_closeAction = static_cast<CloseAction>(settings.value("CloseAction", Ask).toUInt());
    m_motorCommandChannel = settings.value("MotorCommandChannel", "Pelco-D").toString();
    m_motorProtocol   = settings.value("MotorProtocol", "Pelco-D").toString();
    m_motorComPort    = settings.value("MotorComPort", "COM1").toString();
    m_serialServerEnabled = settings.value("SerialServerEnabled", true).toBool();
    m_turntableIpEnabled = settings.value("TurntableIpEnabled", true).toBool();
    m_softwarePtzCalibrationEnabled = settings.value("SoftwarePtzCalibration", false).toBool();
    m_motorSerialEnabled = settings.value("MotorSerialEnabled", true).toBool();
    m_motorIpEnabled = settings.value("MotorIpEnabled", true).toBool();
    m_motorTcpIp = settings.value("MotorTcpIp", "192.168.1.55").toString();
    m_motorTcpPort = static_cast<quint16>(settings.value("MotorTcpPort", 5000).toUInt());
    m_deviceTcpPort = static_cast<quint16>(settings.value("DeviceTcpPort", 8089).toUInt());
    m_visFovDistance = settings.value("VisFovDistance", 4000).toInt();
    m_irFovDistance = settings.value("IrFovDistance", 2000).toInt();

    m_rtspTransport = settings.value("RtspTransport", "tcp").toString();
    m_rtspIoTimeoutMs = settings.value("RtspIoTimeoutMs", 2000).toInt();
    m_rtspStallTimeoutMs = settings.value("RtspStallTimeoutMs", 10000).toInt();
    m_rtspBackoffInitialMs = settings.value("RtspBackoffInitialMs", 1000).toInt();
    m_rtspBackoffMaxMs = settings.value("RtspBackoffMaxMs", 15000).toInt();
    m_rtspBackoffJitterPercent = settings.value("RtspBackoffJitterPercent", 20).toInt();
    m_rtspMaxRetries = settings.value("RtspMaxRetries", 0).toInt();
    m_rtspMinSessionMs = settings.value("RtspMinSessionMs", 2000).toInt();
    m_rtspTcpKeepAlive = settings.value("RtspTcpKeepAlive", true).toBool();
}

// 重新加载：直接委托给 load() 以实现刷新
void ConfigManager::reload()
{
    load();
}

// 保存所有配置到本地存储，并发射变更信号通知其他模块
void ConfigManager::save()
{
    QSettings settings("LSS", "LSS_Video_Manager");
    settings.setValue("SerialIp",   m_serialIp);
    settings.setValue("SerialPort", m_serialPort);
    settings.setValue("MockServerPort", m_mockServerPort);
    settings.setValue("PtzPanOffset", m_ptzPanOffset);
    settings.setValue("PtzTiltOffset", m_ptzTiltOffset);
    settings.setValue("CloseAction", static_cast<quint8>(m_closeAction));
    settings.setValue("MotorCommandChannel", m_motorCommandChannel);
    settings.setValue("MotorProtocol", m_motorProtocol);
    settings.setValue("MotorComPort", m_motorComPort);
    settings.setValue("SerialServerEnabled", m_serialServerEnabled);
    settings.setValue("TurntableIpEnabled", m_turntableIpEnabled);
    settings.setValue("SoftwarePtzCalibration", m_softwarePtzCalibrationEnabled);
    settings.setValue("MotorSerialEnabled", m_motorSerialEnabled);
    settings.setValue("MotorIpEnabled", m_motorIpEnabled);
    settings.setValue("MotorTcpIp", m_motorTcpIp);
    settings.setValue("MotorTcpPort", m_motorTcpPort);
    settings.setValue("DeviceTcpPort", m_deviceTcpPort);
    settings.setValue("VisFovDistance", m_visFovDistance);
    settings.setValue("IrFovDistance", m_irFovDistance);

    settings.setValue("RtspTransport", m_rtspTransport);
    settings.setValue("RtspIoTimeoutMs", m_rtspIoTimeoutMs);
    settings.setValue("RtspStallTimeoutMs", m_rtspStallTimeoutMs);
    settings.setValue("RtspBackoffInitialMs", m_rtspBackoffInitialMs);
    settings.setValue("RtspBackoffMaxMs", m_rtspBackoffMaxMs);
    settings.setValue("RtspBackoffJitterPercent", m_rtspBackoffJitterPercent);
    settings.setValue("RtspMaxRetries", m_rtspMaxRetries);
    settings.setValue("RtspMinSessionMs", m_rtspMinSessionMs);
    settings.setValue("RtspTcpKeepAlive", m_rtspTcpKeepAlive);
}
