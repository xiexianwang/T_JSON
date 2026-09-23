#ifndef MAINPRESENTER_H
#define MAINPRESENTER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <functional>
#include "core/DeviceState.h"
#include "core/DeviceConfig.h"
#include "core/DeviceEntry.h"
#include "infrastructure/rtspthread.h"
#include "infrastructure/tjsonframe.h"
#include "PresenterStateViewService.h"

class IMainView;
class DeviceContext;
class ConfigManager;
class PresenterDeviceService;
class PresenterMotorService;
class PresenterMapService;
class DeviceStateService;
class PresenterAiViewService;
class PresenterMediaService;
class DeviceControlService;

// ============================================================================
// MainPresenter - MainWindow 的控制器 (MVP 模式中的 Presenter)
// 只依赖 IMainView 窄接口，不再直接操作 Ui::MainWindow 控件
// ============================================================================
class MainPresenter : public QObject
{
    Q_OBJECT
public:
    explicit MainPresenter(IMainView* view, ConfigManager* cfg, QObject *parent = nullptr);
    ~MainPresenter() override;

    // --- 供 View 调用的命令接口 ---
    void connectToDevice(const QString& ip, quint16 port);
    void disconnectDevice();
    void ptzMove(int direction);
    void ptzStop();

    // --- 镜头控制（op: 0=ZoomIn, 1=ZoomOut, 2=FocusIn, 3=FocusOut） ---
    void lensMove(int op);
    void lensStop();

    // --- 提取的业务按钮逻辑 ---
    void on_btnPtzMoveTo_clicked();
    void on_btnPtzMoveToGps_clicked();
    void on_btnPanZeroCalib_clicked();
    void on_btnSetLocation_clicked();
    void on_btnGetImageParams_clicked();

    // --- 电机通道初始化/切换 ---
    void initMotorChannel();
    void applyMotorChannel();
    void applyMotorChannelForDevice(const QString& deviceId);
    bool isMotorSerialOpen() const;
    bool isMotorTcpOpen() const;
    // 当前设备的电机协议 / 指令通道（未连接时回退全局默认）
    QString motorProtocol() const;
    QString motorCommandChannel() const;

    // --- PTZ 转发服务 ---
    void initPtzForwarder();
    void initPtzForwarderForDevice(const QString& deviceId);

    // --- 雨刷电机控制 ---
    void onWiperStart();
    void onWiperStop();
    void onWiperJogLeft();
    void onWiperJogRight();
    void onWiperJogStop();
    void onWiperZeroCalib();
    void onWiperMode();
    void onWiperSilent();
    void onWiperCurrentSet();
    void checkMotorMode();
    void readMotorCurrent();   // 读实际电流（MODBUS-RTU 寄存器 0x000D）

    // --- 预置位与复位 ---
    void on_btnCallPreset_clicked();
    void on_btnSetPreset_clicked();
    void on_btnDelPreset_clicked();
    void on_btnPtzReset_clicked();

    // --- 附加功能开关 ---
    void onCheckDigitalZoomToggled(bool checked);
    void onCheckAutoZoomToggled(bool checked);
    void onCheckCaptureUploadToggled(bool checked);
    void onCheckPosResetToggled(bool checked);

    // --- 框选/点选跟踪 ---
    void onVideoSelection(const QString& deviceId, int cx, int cy, int pw, int ph);

    // --- 工作模式/算法模型/显示模式 ---
    void onComboWorkModeChanged(int index);
    void sendAlgoModel(int model);
    void onComboDisplayModeChanged(int index);

    // --- 多设备支持（设备树驱动，id 为稳定标识） ---
    void onDeviceActivated(const QString& deviceId, const DeviceEntry& entry);
    void onDeviceRemoved(const QString& deviceId);
    void onDeviceToggleConnect(const QString& deviceId, const DeviceEntry& entry);
    QString currentDeviceId() const;

    // --- 切换当前设备焦点（点击视频格）：不触碰 TCP/RTSP，避免视频闪断 ---
    void selectDevice(const QString& deviceId);

    // --- 视频流状态查询/关闭 ---
    bool isVideoStreamRunning() const;
    void closeVideoStream();
    bool isDeviceConnected() const;

signals:
    void motorModeChanged(bool isManual);
    void motorSerialErrorOccurred(const QString& msg);
    void motorTcpErrorOccurred(const QString& msg);
    void motorSilentChanged(bool isSilent);
    void motorCurrentChanged(int run, int hold, int delay);
    void rtspStatsChanged(const QString& deviceId, const RtspThread::Stats& stats);
    void commandSentToLog(const QString& serialType, const QByteArray& data);
    void deviceConfigChanged(const QString& deviceId, const DeviceConfig& cfg);
    // 当前焦点设备变化（含置空），供 View 更新设备树标记
    void currentDeviceChanged(const QString& deviceId);

private:
    IMainView* m_view;
    ConfigManager* m_cfg;

    FrameType m_lastAckFrameType = FrameType::Status;

    // ── 设备状态缓存 ──
    StateViewCache m_cache;
    bool m_updatingFromDevice = false;

    double m_deviceHeight = 0;

    void showAck(quint8 statusCode);
    void resetDeviceStateCache();
    StateViewCache currentStateViewCache() const;
    void applyStateViewCache(const StateViewCache& cache);

    PresenterDeviceService* m_deviceService;
    PresenterMotorService* m_motorService;
    PresenterMapService* m_mapService;
    DeviceStateService* m_stateService;
    PresenterStateViewService* m_stateViewService;
    PresenterAiViewService* m_aiViewService;
    PresenterMediaService* m_mediaService;
    DeviceControlService* m_controlService;

    DeviceContext* currentDevice() const;

    void saveDeviceSwitchConfig(const std::function<void(DeviceConfig&, bool)>& setter, bool value);
    void refreshSwitchView(const QString& deviceId);

private slots:
    void onDeviceConnected(const QString& deviceId);
    void onDeviceDisconnected(const QString& deviceId);
    void onDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void onDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc);
    void onDeviceAiTimeout(const QString& deviceId);
    void onDeviceSwitched();
    void updateAiInfoFromJson(const QString& deviceId, const QJsonObject& aiDoc);
};

#endif // MAINPRESENTER_H
