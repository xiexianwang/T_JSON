#ifndef PRESENTERDEVICESERVICE_H
#define PRESENTERDEVICESERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QByteArray>
#include <QRect>
#include <memory>
#include "core/DeviceState.h"
#include "core/DeviceConfig.h"
#include "core/DeviceEntry.h"
#include "infrastructure/rtspthread.h"

class IMainView;
class ConfigManager;
class DeviceContext;
class DeviceSessionService;
class PresenterMotorService;

// ============================================================================
// PresenterDeviceService - 设备生命周期与状态服务
// 职责：设备激活/连接/断开/删除、EventBus 订阅、初始化参数下发、
//       状态→视图模型转换（不直接操作 Widget）、信号解绑。
// 不负责：PTZ/镜头/电机细节、地图坐标计算、具体 Widget 操作。
//
// 多设备语义：同时只允许一个"当前设备"（仪表盘/地图/电机跟随它），
//             但允许多个设备保持 TCP/RTSP 在线并各自显示视频。
// ============================================================================
class PresenterDeviceService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterDeviceService(IMainView* view, ConfigManager* cfg, QObject *parent = nullptr);
    ~PresenterDeviceService() override;

    // ================= 设备激活 / 连接 / 断开 =================
    // 设为当前设备：不断开旧设备（多设备并行在线），仅切换控制与展示焦点
    void activateDevice(const QString& deviceId, const DeviceEntry& entry);
    // 仅切换当前设备焦点：不触碰 TCP/RTSP（用于点击视频格）
    void focusDevice(const QString& deviceId);
    // 连接/断开切换（保持当前设备语义）
    void toggleDeviceConnect(const QString& deviceId, const DeviceEntry& entry);
    // 断开指定设备但保留其上下文与视频槽位
    void disconnectDevice(const QString& deviceId);
    bool isDeviceConnected(const QString& deviceId) const;

    // ================= 设备删除 =================
    void removeDevice(const QString& deviceId);

    // ================= 当前设备管理 =================
    QString currentDeviceId() const { return m_currentDeviceId; }
    DeviceContext* currentDevice() const;

    // ================= 电机状态查询（供 DialogService 使用） =================
    bool isMotorSerialOpen(const QString& deviceId) const;
    bool isMotorTcpOpen(const QString& deviceId) const;

    // ================= 初始化 EventBus 连接 =================
    void setupEventBus();

    // ================= 外部服务引用（由 MainPresenter 注入） =================
    void setMotorService(PresenterMotorService* svc) { m_motorService = svc; }

signals:
    // ---- View 层信号（MainPresenter 转发给 IMainView） ----
    void deviceConnected(const QString& deviceId);
    void deviceDisconnected(const QString& deviceId);
    void deviceFrameReady(const QString& deviceId, const QImage& frame);
    void deviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void deviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc);
    void deviceAiTimeout(const QString& deviceId);
    void deviceError(const QString& deviceId, const QString& errorMsg);
    void rtspOpened(const QString& deviceId);
    void rtspError(const QString& deviceId, const QString& msg);
    void imageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location);
    void ackReceived(const QString& deviceId, quint8 statusCode);
    void deviceReconnecting(const QString& deviceId, int attempt, int maxRetries);
    void deviceReconnectFailed(const QString& deviceId);
    // 设备切换后通知 MainPresenter 刷新仪表盘
    void deviceSwitched();
    // 视频槽位已满，无法为新设备分配画面
    void videoSlotUnavailable(const QString& deviceId);
    void motorModeChanged(const QString& deviceId, bool isManual);
    void motorSerialError(const QString& deviceId, const QString& msg);
    void motorTcpError(const QString& deviceId, const QString& msg);
    void motorSilentChanged(const QString& deviceId, bool isSilent);
    void motorCurrentChanged(const QString& deviceId, int run, int hold, int delay);
    void commandSent(const QString& deviceId, const QString& serialType, const QByteArray& data);
    void rtspStatsChanged(const QString& deviceId, const RtspThread::Stats& stats);

private:
    bool acceptsEvent(const QString& deviceId, quint64 generation) const;
    // 设备电机信号连接管理（切换设备时解绑/重绑）
    void connectDeviceSignals(DeviceContext* ctx);
    void disconnectDeviceSignals();
    // 切换当前设备时的公共清理（地图/识别/跟踪/旧设备电机）
    void resetForSwitch(DeviceContext* oldCtx);
    // 确保上下文存在并应用设备条目配置
    DeviceContext* ensureContext(const QString& deviceId, const DeviceEntry& entry);

    IMainView* m_view;
    ConfigManager* m_cfg;
    DeviceSessionService* m_session;
    PresenterMotorService* m_motorService = nullptr;
    QString m_currentDeviceId;

    // 电机信号连接
    QMetaObject::Connection m_motorModeConn;
    QMetaObject::Connection m_motorErrorConn;
    QMetaObject::Connection m_motorTcpErrorConn;
    QMetaObject::Connection m_motorSilentConn;
    QMetaObject::Connection m_motorCurrentConn;
    QMetaObject::Connection m_commandLogConn;
    QMetaObject::Connection m_rtspStatsConn;
};

#endif // PRESENTERDEVICESERVICE_H
