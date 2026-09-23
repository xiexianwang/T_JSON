// ============================================================
// 文件: configmanager.h
// 描述: 配置管理器模块。通过 QSettings 将云台(PTZ)、镜头(Lens)
//       和相机(Camera)参数持久化到本地存储，并提供运行时的读写接口。
// ============================================================

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QMap>

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    enum CloseAction { Ask = 0, Exit = 1, Minimize = 2 };

    explicit ConfigManager(QObject *parent = nullptr);

    void load();                // 从本地存储加载所有配置
    void reload();              // 重新加载配置（委托给 load）
    void save();                // 将所有配置写入本地存储并发射变更信号

    QString serialIp() const { return m_serialIp; }        // 串口服务器 IP
    quint16 serialPort() const { return m_serialPort; }    // 串口服务器端口
    void setSerialIp(const QString& ip) { m_serialIp = ip; }
    void setSerialPort(quint16 port) { m_serialPort = port; }
    quint16 mockServerPort() const { return m_mockServerPort; }
    double ptzPanOffset() const { return m_ptzPanOffset; }
    void setPtzPanOffset(double offset) { m_ptzPanOffset = offset; }
    double ptzTiltOffset() const { return m_ptzTiltOffset; }
    void setPtzTiltOffset(double offset) { m_ptzTiltOffset = offset; }
    void setMockServerPort(quint16 port) { m_mockServerPort = port; }

    CloseAction closeAction() const { return m_closeAction; }
    void setCloseAction(CloseAction action) { m_closeAction = action; }
    QString motorCommandChannel() const { return m_motorCommandChannel; }
    void setMotorCommandChannel(const QString& channel) { m_motorCommandChannel = channel; }
    QString motorProtocol() const { return m_motorProtocol; }
    void setMotorProtocol(const QString& proto) { m_motorProtocol = proto; }
    QString motorComPort() const { return m_motorComPort; }
    void setMotorComPort(const QString& port) { m_motorComPort = port; }
    bool serialServerEnabled() const { return m_serialServerEnabled; }
    void setSerialServerEnabled(bool enabled) { m_serialServerEnabled = enabled; }
    bool turntableIpEnabled() const { return m_turntableIpEnabled; }
    void setTurntableIpEnabled(bool enabled) { m_turntableIpEnabled = enabled; }
    bool softwarePtzCalibrationEnabled() const { return m_softwarePtzCalibrationEnabled; }
    void setSoftwarePtzCalibrationEnabled(bool enabled) { m_softwarePtzCalibrationEnabled = enabled; }
    
    bool motorSerialEnabled() const { return m_motorSerialEnabled; }
    void setMotorSerialEnabled(bool enabled) { m_motorSerialEnabled = enabled; }
    bool motorIpEnabled() const { return m_motorIpEnabled; }
    void setMotorIpEnabled(bool enabled) { m_motorIpEnabled = enabled; }
    QString motorTcpIp() const { return m_motorTcpIp; }
    void setMotorTcpIp(const QString& ip) { m_motorTcpIp = ip; }
    quint16 motorTcpPort() const { return m_motorTcpPort; }
    void setMotorTcpPort(quint16 port) { m_motorTcpPort = port; }
    quint16 deviceTcpPort() const { return m_deviceTcpPort; }
    void setDeviceTcpPort(quint16 port) { m_deviceTcpPort = port; }

    // ---- RTSP 拉流参数（可配置，缺省为工业常用值） ----
    QString rtspTransport() const { return m_rtspTransport; }
    void setRtspTransport(const QString& t) { m_rtspTransport = t; }
    int rtspIoTimeoutMs() const { return m_rtspIoTimeoutMs; }
    void setRtspIoTimeoutMs(int v) { m_rtspIoTimeoutMs = v; }
    int rtspStallTimeoutMs() const { return m_rtspStallTimeoutMs; }
    void setRtspStallTimeoutMs(int v) { m_rtspStallTimeoutMs = v; }
    int rtspBackoffInitialMs() const { return m_rtspBackoffInitialMs; }
    void setRtspBackoffInitialMs(int v) { m_rtspBackoffInitialMs = v; }
    int rtspBackoffMaxMs() const { return m_rtspBackoffMaxMs; }
    void setRtspBackoffMaxMs(int v) { m_rtspBackoffMaxMs = v; }
    int rtspBackoffJitterPercent() const { return m_rtspBackoffJitterPercent; }
    void setRtspBackoffJitterPercent(int v) { m_rtspBackoffJitterPercent = v; }
    int rtspMaxRetries() const { return m_rtspMaxRetries; }
    void setRtspMaxRetries(int v) { m_rtspMaxRetries = v; }
    int rtspMinSessionMs() const { return m_rtspMinSessionMs; }
    void setRtspMinSessionMs(int v) { m_rtspMinSessionMs = v; }
    bool rtspTcpKeepAlive() const { return m_rtspTcpKeepAlive; }
    void setRtspTcpKeepAlive(bool v) { m_rtspTcpKeepAlive = v; }
    int visFovDistance() const { return m_visFovDistance; }
    void setVisFovDistance(int dist) { m_visFovDistance = dist; }
    int irFovDistance() const { return m_irFovDistance; }
    void setIrFovDistance(int dist) { m_irFovDistance = dist; }

private:
    QString m_serialIp = "192.168.1.66";   // 串口服务器 IP 地址
    quint16 m_serialPort = 4001;
    quint16 m_mockServerPort = 5001;
    double m_ptzPanOffset = 0.0;
    double m_ptzTiltOffset = 0.0;           // 串口服务器端口号
    CloseAction m_closeAction = Ask;
    QString m_motorCommandChannel = "Pelco-D";
    QString m_motorProtocol = "Pelco-D";
    QString m_motorComPort = "COM1";
    bool m_serialServerEnabled = true;
    bool m_turntableIpEnabled = true;
    bool m_softwarePtzCalibrationEnabled = false;
    bool m_motorSerialEnabled = true;
    bool m_motorIpEnabled = true;
    QString m_motorTcpIp = "192.168.1.55";
    quint16 m_motorTcpPort = 5000;
    quint16 m_deviceTcpPort = 8089;
    int m_visFovDistance = 4000;
    int m_irFovDistance = 2000;

    // RTSP 拉流参数默认值（工业常用）
    QString m_rtspTransport = "tcp";
    int m_rtspIoTimeoutMs = 2000;
    int m_rtspStallTimeoutMs = 10000;
    int m_rtspBackoffInitialMs = 1000;
    int m_rtspBackoffMaxMs = 15000;
    int m_rtspBackoffJitterPercent = 20;
    int m_rtspMaxRetries = 0;          // 0 = 无限重试
    int m_rtspMinSessionMs = 2000;
    bool m_rtspTcpKeepAlive = true;
};

#endif // CONFIGMANAGER_H
