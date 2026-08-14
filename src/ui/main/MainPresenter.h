#ifndef MAINPRESENTER_H
#define MAINPRESENTER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QRect>
#include <QDateTime>
#include <memory>
#include "core/DeviceState.h"
#include "infrastructure/tjsonclient.h"

class IMainView;
class DeviceContext;
class ConfigManager;
class DeviceController;
class TJsonClient;
class RtspThread;
class PtzForwarder;

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
    void initMotorChannel();        // 根据配置自动打开电机通道（启动时）
    void applyMotorChannel();       // 设置页协议变更后重新打开电机通道
    bool isMotorSerialOpen() const; // MODBUS-RTU 串口是否就绪
    bool isMotorTcpOpen() const;    // STM32-TCP 通道是否就绪

    // --- PTZ 转发服务（Pelco-D 串口服务器） ---
    void initPtzForwarder();        // 启动 PTZ 转发 + 应用角度偏移（启动/设置变更后调用）

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
    void checkMotorMode();          // 查询电机当前模式（手动/自动）

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
    QString currentDeviceId() const { return m_currentDeviceId; }

    // --- 视频流状态查询/关闭 ---
    bool isVideoStreamRunning() const;
    void closeVideoStream();
    bool isDeviceConnected() const;

signals:
    // --- 底层信号经 Presenter 转发给 View（View 不直接连接底层组件） ---
    void motorModeChanged(bool isManual);
    void motorSerialErrorOccurred(const QString& msg);
    void motorSilentChanged(bool isSilent);
    void commandSentToLog(const QString& serialType, const QByteArray& data);

private:
    IMainView* m_view;
    ConfigManager* m_cfg;
    
    QString m_currentDeviceId;

    // ACK 处理：记录最近一次发送的帧类型，用于判断 ACK 状态码含义
    FrameType m_lastAckFrameType = FrameType::Status;

    // ── 设备状态缓存（原 MainWindow 字段，随业务收敛迁移至此） ──
    double m_currentVisZoom = 1.0;      // 当前可见光镜头倍率
    double m_currentIrZoom = 1.0;       // 当前红外镜头倍率
    double m_currentTilt = 0.0;         // 当前云台俯仰角（原始值，用于地图计算）
    int m_currentPipShow = 0;           // 当前画中画显示模式（0~4）
    int m_previousWorkMode = 0;         // 最近上报的 WorkMode
    bool m_workModeInitialized = false;
    bool m_displayModeInitialized = false;
    bool m_algoModelInitialized = false;
    int m_previousAlgoModel = 0;        // 最近上报的 Model
    int m_currentAlgoModel = 0;
    int m_previousDisplayMode = 0;      // 最近上报的 PipShow
    int m_currentResX = 2688;           // 当前可见光实际水平分辨率
    int m_currentResY = 1520;           // 当前可见光实际垂直分辨率
    bool m_updatingFromDevice = false;  // 防递归更新标志

    // ── 跟踪状态管理（地图目标/轨迹逻辑） ──
    struct TrackState {
        QString id;
        double lat = 0, lon = 0;
        int cls = 0;
        QDateTime lostSince;
        double prevLat = 0, prevLon = 0;
        QDateTime prevTime;

        double plotLat = 0, plotLon = 0;
        double plotHeading = -1;
        QDateTime plotTime;
    };
    TrackState m_track;

    // ── AI 目标距离缓存（用于 ZoomInfo 无激光测距时回退显示） ──
    double m_lastAiDist = 0;
    bool m_lastAiDistEstimated = false;

    // ── AIInfo 超时清理 ──
    QDateTime m_lastAiInfoTime;

    // ── 设备高度（手动下发经纬度时更新） ──
    double m_deviceHeight = 0;

    // ── 首次连接自动打开 RTSP 标记 ──
    bool m_rtspEverOpened = false;

    void setupEventBus();
    void showAck(quint8 statusCode);
    void updateLensStats();
    double calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel);
    int currentAlgoModel() const { return m_currentAlgoModel; }

    // Presenter 内部访问当前设备的底层组件（View 不得直接调用）
    DeviceController* motorController() const;
    TJsonClient* tcpClient() const;
    RtspThread* videoStream() const;
    PtzForwarder* ptzForwarder() const;

private slots:
    void onDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state);
    void onDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc);
    void updateStatusFromState(const DeviceState& state);
    void updateAiInfoFromJson(const QJsonObject& aiDoc);
    void updateMapTargets(const QJsonObject& doc, int workMode);
    void updateMapDevicePosition(const DeviceState& state);
    void onDeviceAiTimeout(const QString& deviceId);
};

#endif // MAINPRESENTER_H
