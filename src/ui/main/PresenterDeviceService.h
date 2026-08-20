#ifndef PRESENTERDEVICESERVICE_H
#define PRESENTERDEVICESERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QByteArray>
#include <QRect>
#include <memory>
#include "core/DeviceState.h"

class MainPresenter;
class IMainView;
class ConfigManager;
class DeviceContext;
class DeviceSessionService;

// ============================================================================
// PresenterDeviceService - 设备生命周期与状态服务
// 职责：设备连接/断开/重连、设备切换、EventBus 订阅、初始化参数下发、
//       状态→视图模型转换（不直接操作 Widget）、信号解绑。
// 不负责：PTZ/镜头/电机细节、地图坐标计算、具体 Widget 操作。
// ============================================================================
class PresenterDeviceService : public QObject
{
    Q_OBJECT
public:
    explicit PresenterDeviceService(MainPresenter* parentPresenter, IMainView* view, ConfigManager* cfg, QObject *parent = nullptr);
    ~PresenterDeviceService() override;

    // ================= 设备连接/断开 =================
    void connectToDevice(const QString& deviceId, const QString& ip, quint16 port);
    void disconnectDevice(const QString& deviceId);
    void toggleDeviceConnect(const QString& ip);
    bool isDeviceConnected(const QString& deviceId) const;

    // ================= 视频流 =================
    void startVideoStream(const QString& deviceId, const QString& url);
    void stopVideo(const QString& deviceId);
    bool isVideoRunning(const QString& deviceId) const;

    // ================= 设备切换与删除 =================
    void switchToDevice(const QString& newDeviceId, const QString& ip, const QString& rtspUrl);
    void removeDevice(const QString& ip);

    // ================= 当前设备管理 =================
    QString currentDeviceId() const { return m_currentDeviceId; }
    DeviceContext* currentDevice() const;

    // ================= 电机状态查询（供 DialogService 使用） =================
    bool isMotorSerialOpen(const QString& deviceId) const;
    bool isMotorTcpOpen(const QString& deviceId) const;

    // ================= 初始化 EventBus 连接 =================
    void setupEventBus();

signals:
    // ---- View 层信号（MainPresenter 转发给 IMainView） ----
    void deviceConnected();
    void deviceDisconnected();
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

private:
    // 设备电机信号连接管理（切换设备时解绑/重绑）
    void connectDeviceSignals(DeviceContext* ctx);
    void disconnectDeviceSignals();

    MainPresenter* m_presenter;
    IMainView* m_view;
    ConfigManager* m_cfg;
    DeviceSessionService* m_session;
    QString m_currentDeviceId;

    // 电机信号连接
    QMetaObject::Connection m_motorModeConn;
    QMetaObject::Connection m_motorErrorConn;
    QMetaObject::Connection m_motorTcpErrorConn;
    QMetaObject::Connection m_motorSilentConn;
    QMetaObject::Connection m_commandLogConn;
};

#endif // PRESENTERDEVICESERVICE_H
