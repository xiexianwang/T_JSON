#ifndef DEVICECONTEXT_H
#define DEVICECONTEXT_H

#include <QObject>
#include <memory>
#include <QString>
#include <QByteArray>
#include "core/DeviceState.h"
#include "core/DeviceTypes.h"
#include "infrastructure/tjsonclient.h"
#include "infrastructure/devicecontroller.h"
#include "infrastructure/rtspthread.h"
#include "infrastructure/ptzforwarder.h"
#include "infrastructure/configmanager.h"
#include <QTimer>

// ============================================================================
// DeviceContext - 设备运行时上下文 (聚合根)
// 代表一个真实物理设备的所有控制模块与状态。
// 只对外暴露设备业务 API 与状态，不再公开底层组件
// (TJsonClient / DeviceController / RtspThread / PtzForwarder)。
// ============================================================================
class DeviceContext : public QObject
{
    Q_OBJECT
public:
    explicit DeviceContext(const QString& deviceId, ConfigManager* cfg, QObject *parent = nullptr);
    ~DeviceContext() override;

    QString deviceId() const { return m_deviceId; }
    void setSessionGeneration(quint64 generation) { m_sessionGeneration = generation; }
    quint64 sessionGeneration() const { return m_sessionGeneration; }

    // PipShow 映射表：combo 索引 → 设备实际值（复用 DeviceController 静态映射）
    static int pipShowToComboIndex(int pipShow) { return DeviceController::pipShowToComboIndex(pipShow); }

    // ================= 生命周期与状态 =================
    enum class State {
        Active,
        ShuttingDown,
        Stopped
    };
    struct ShutdownResult {
        bool alreadyStopped = false;
        bool rtspStopped = false;
        bool ptzStopped = false;
        bool motorStopped = false;
        bool networkStopped = false;
        QString error;

        bool succeeded() const
        {
            return error.isEmpty() && rtspStopped && ptzStopped
                && motorStopped && networkStopped;
        }
    };

    ShutdownResult shutdown();
    State lifecycleState() const { return m_lifecycleState; }
    bool isActive() const { return m_lifecycleState == State::Active; }

    // ================= 连接与网络 =================
    void connectDevice(const QString& ip, quint16 port);   // 建立 TCP 连接
    void disconnectDevice();                               // 完整停止（网络+视频+电机+PTZ 转发）
    void disconnectNetwork();                              // 仅断开 TCP（停止自动重连）
    bool isConnected() const;                              // TCP 是否已连接

    // ================= 视频流 =================
    void startVideo(const QString& url);
    void stopVideo();
    bool isVideoRunning() const;

    // ================= 云台控制 (Pelco-D) =================
    void ptzMove(PtzDir dir);
    void ptzStop();
    void ptzMoveTo(double pan, double tilt);
    void ptzSetZero();

    // ================= 镜头控制 =================
    void lensZoomIn(int target);
    void lensZoomOut(int target);
    void lensFocusIn(int target);
    void lensFocusOut(int target);
    void lensStop();

    // ================= 图像参数 / 工作模式 / 算法 / 显示 =================
    void queryImageParams();
    void setWorkMode(int mode);
    void setAlgoModel(int model);
    void setDisplayMode(int mode);
    void setLocation(const QString& lat, const QString& lon);

    // ================= 附加功能开关 =================
    void setDigitalZoom(bool enable);
    void setAutoZoom(bool enable);
    void setCaptureUpload(bool enable);
    void posReset(bool enable);

    // ================= 框选/点选跟踪 =================
    void setPointTrack(int centerX, int centerY);
    void setBoxTrack(int centerX, int centerY, int width, int height);

    // ================= 预置位 =================
    void setPreset(int preset);
    void callPreset(int preset);
    void delPreset(int preset);

    // ================= 电机通道管理 =================
    bool openMotorSerial(const QString& portName);
    void closeMotorSerial();
    bool isMotorSerialOpen() const;
    void openMotorTcp();
    void closeMotorTcp();
    bool isMotorTcpOpen() const;

    // ================= 雨刷电机控制 =================
    void motorStart();
    void motorStop();
    void motorWiperStop();
    void motorReturnZero();
    void motorJogLeft();
    void motorJogRight();
    void motorZeroCalib();
    void motorCheckMode();
    void motorToggleMode();
    void motorToggleSilentMode();
    void motorSetCurrent(int ma);

    // ================= PTZ 转发服务 =================
    void startPtzForwarder(const QString& ptzIp, quint16 ptzPort, quint16 mockServerPort);
    void setPtzOffsets(double panOffset, double tiltOffset);
    void flushZeroPosition();

    // ================= 状态 =================
    DeviceState* state() const { return m_state.get(); }

signals:
    // 底层电机/指令日志信号转发（经 DeviceContext 收口，View 不直连底层）
    void commandSent(const QString& serialType, const QByteArray& data);
    void motorModeResult(bool isManual);
    void motorSilentResult(bool isSilent);
    void motorSerialError(const QString& msg);
    void motorTcpError(const QString& msg);

private:
    QString m_deviceId;
    quint64 m_sessionGeneration = 0;
    ConfigManager* m_cfg;

    // 底层驱动与组件（聚合根内部持有，不对外暴露）
    State m_lifecycleState = State::Active;
    TJsonClient* m_tcp;
    DeviceController* m_motor;
    RtspThread* m_video;
    PtzForwarder* m_ptz;
    QTimer* m_sysParamTimer;
    QTimer* m_aiCleanupTimer;
    void setupTimers();

    // 设备数据状态
    std::shared_ptr<DeviceState> m_state;
};

#endif // DEVICECONTEXT_H
