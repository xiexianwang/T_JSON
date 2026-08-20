#ifndef MAINPRESENTER_H
#define MAINPRESENTER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QRect>
#include <QDateTime>
#include <memory>
#include "core/DeviceState.h"
#include "infrastructure/tjsonframe.h"
#include "PresenterStateViewService.h"

class IMainView;
class DeviceContext;
class ConfigManager;
class PresenterDeviceService;
class PresenterMotorService;
class PresenterMapService;
class DeviceStateService;

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
    void on_btnConnect_clicked();
    void startVideoStream(const QString& url);
    void on_btnCancelConnect_clicked();
    void on_btnVideoConnect_clicked();
    void on_btnVideoDisconnect_clicked();
    void on_btnPtzMoveTo_clicked();
    void on_btnPtzMoveToGps_clicked();
    void on_btnPanZeroCalib_clicked();
    void on_btnSetLocation_clicked();
    void on_btnGetImageParams_clicked();

    // --- 电机通道初始化/切换 ---
    void initMotorChannel();
    void applyMotorChannel();
    bool isMotorSerialOpen() const;
    bool isMotorTcpOpen() const;

    // --- PTZ 转发服务 ---
    void initPtzForwarder();

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

    // --- 多设备支持 ---
    void onDeviceDoubleClicked(const QString& name, const QString& ip, const QString& rtspUrl);
    void onDeviceRemoved(const QString& ip);
    void onDeviceToggleConnect(const QString& ip);
    QString currentDeviceId() const;

    // --- 视频流状态查询/关闭 ---
    bool isVideoStreamRunning() const;
    void closeVideoStream();
    bool isDeviceConnected() const;

signals:
    void motorModeChanged(bool isManual);
    void motorSerialErrorOccurred(const QString& msg);
    void motorTcpErrorOccurred(const QString& msg);
    void motorSilentChanged(bool isSilent);
    void commandSentToLog(const QString& serialType, const QByteArray& data);

private:
    IMainView* m_view;
    ConfigManager* m_cfg;

    FrameType m_lastAckFrameType = FrameType::Status;

    // ── 设备状态缓存 ──
    double m_currentVisZoom = 1.0;
    double m_currentIrZoom = 1.0;
    double m_currentTilt = 0.0;
    int m_currentPipShow = 0;
    int m_previousWorkMode = 0;
    bool m_workModeInitialized = false;
    bool m_displayModeInitialized = false;
    bool m_algoModelInitialized = false;
    int m_previousAlgoModel = 0;
    int m_currentAlgoModel = 0;
    int m_previousDisplayMode = 0;
    int m_currentResX = 2688;
    int m_currentResY = 1520;
    bool m_updatingFromDevice = false;

    double m_lastAiDist = 0;
    bool m_lastAiDistEstimated = false;
    QDateTime m_lastAiInfoTime;

    double m_deviceHeight = 0;
    bool m_rtspEverOpened = false;

    void showAck(quint8 statusCode);
    double calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel);
    int currentAlgoModel() const { return m_currentAlgoModel; }
    void resetDeviceStateCache();
    StateViewCache currentStateViewCache() const;
    void applyStateViewCache(const StateViewCache& cache);

    PresenterDeviceService* m_deviceService;
    PresenterMotorService* m_motorService;
    PresenterMapService* m_mapService;
    DeviceStateService* m_stateService;
    PresenterStateViewService* m_stateViewService;

    DeviceContext* currentDevice() const;

private slots:
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void onDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc);
    void onDeviceAiTimeout(const QString& deviceId);
    void onDeviceSwitched();
    void updateAiInfoFromJson(const QJsonObject& aiDoc);
};

#endif // MAINPRESENTER_H
