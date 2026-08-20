#ifndef DEVICECONTROLSERVICE_H
#define DEVICECONTROLSERVICE_H

#include <QObject>
#include <QString>

class ConfigManager;

// ============================================================================
// DeviceControlService - 设备命令编排服务
// 接收 DeviceId，通过 DeviceManager 路由到对应 DeviceContext。
// 覆盖：PTZ 定位、位置设置、图像参数、工作模式、算法/显示模式、
//       附加功能开关（数字变倍/自动变倍/抓拍上传/位置复位）、框选跟踪。
// PTZ 方向/镜头/预置位/雨刷电机 由 PresenterMotorService 负责。
// ============================================================================
class DeviceControlService : public QObject
{
    Q_OBJECT
public:
    explicit DeviceControlService(ConfigManager* cfg, QObject* parent = nullptr);

    // ================= PTZ 定位 =================
    void ptzMoveTo(const QString& deviceId, double pan, double tilt);
    void ptzSetZero(const QString& deviceId);
    void setPtzOffsets(const QString& deviceId, double panOffset, double tiltOffset);
    void flushZeroPosition(const QString& deviceId);

    // ================= 位置设置 =================
    void setLocation(const QString& deviceId, const QString& lat, const QString& lon);

    // ================= 图像参数 / 工作模式 / 算法 / 显示 =================
    void queryImageParams(const QString& deviceId);
    void setWorkMode(const QString& deviceId, int mode);
    void setAlgoModel(const QString& deviceId, int model);
    void setDisplayMode(const QString& deviceId, int mode);

    // ================= 附加功能开关 =================
    void setDigitalZoom(const QString& deviceId, bool enable);
    void setAutoZoom(const QString& deviceId, bool enable);
    void setCaptureUpload(const QString& deviceId, bool enable);
    void setPosReset(const QString& deviceId, bool enable);

    // ================= 框选/点选跟踪 =================
    void setPointTrack(const QString& deviceId, int centerX, int centerY);
    void setBoxTrack(const QString& deviceId, int centerX, int centerY, int width, int height);

private:
    ConfigManager* m_cfg = nullptr;
};

#endif // DEVICECONTROLSERVICE_H
