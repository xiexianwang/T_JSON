//============================================================================
// mainwindow.h - T-JSON 主窗口头文件
// 定义主界面类 MainWindow，负责整体 UI 布局、事件响应、设备交互调度
// 以及视频显示、地图展示、云台控制、AI 识别/跟踪结果展示等功能
//============================================================================
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <QDialog>
#include <QSystemTrayIcon>
#include <QMenu>
#include "infrastructure/configmanager.h"
#include "ui/main/MainPresenter.h"
#include "ui/main/IMainView.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

class VideoWidget;
class MapWidget;
class CmdLogDialog;
class VideoGridWidget;
class DeviceTreeWidget;
class MainWindowNavigation;
class MainWindowDialogService;
class MainWindowLayoutService;
class MainWindowSystemService;
class MainWindowControlService;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

//============================================================================
// MainWindow - 应用程序主窗口
// 整合设备连接/断连、视频流显示、AI 识别列表、目标跟踪状态、
// 地图定位/视场角叠加、云台(Pelco-D)与镜头控制等多个子系统。
// 通过 Qt 信号-槽机制将底层 TJsonClient / DeviceController 的异步
// 事件转化为 UI 更新，是前后端通信的调度中枢。
//============================================================================
class MainWindow : public QMainWindow, public IMainView
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    // ---- IMainView 接口实现 ----
    QWidget* asWidget() override;
    void showStatusMessage(const QString& msg, int timeoutMs = 0) override;

    QString targetPanText() const override;
    QString targetTiltText() const override;
    QString targetLonText() const override;
    QString targetLatText() const override;
    QString targetAltText() const override;
    QString setLatText() const override;
    QString setLonText() const override;
    QString setHeightText() const override;
    int wiperRunCurrent() const override;
    int wiperHoldCurrent() const override;
    int wiperHoldDelay() const override;
    int presetValue() const override;
    int workModeIndex() const override;
    int algoModel2Index() const override;
    int displayModeIndex() const override;

    QString statLatitudeText() const override;
    QString statLongitudeText() const override;
    QString statPanAngleText() const override;
    QString statTiltAngleText() const override;

    void showDeviceState(const QString& lat, const QString& lon,
                         const QString& height, const QString& pan, const QString& tilt) override;
    void showLensStats(double visZoom, double visFocal, double visHfov,
                       double irZoom, double irFocal, double irHfov) override;
    void setIdentifyCount(const QString& text) override;
    void clearIdentifyTable() override;
    void addIdentifyRow(const QString& id, const QString& typeName, double dist,
                        const QString& pos, const QString& miss) override;
    void showTrackStatus(const QString& text, const QString& state) override;
    void setTrackDistance(const QString& text) override;
    void setTrackPos(const QString& text) override;
    void setTrackMissDistance(const QString& text) override;
    void setTrackTargetType(const QString& text) override;
    void setTrackAngle(const QString& text) override;
    void showImageParams(const QString& resolution, const QString& bitrate,
                         const QString& codec, const QString& workMode,
                         const QString& pipShow, const QString& algoModel,
                         const QString& maxVisFL, const QString& maxIRFL) override;
    void setAlgoModel1Index(int high) override;
    void setAlgoModel2Index(int low) override;
    void setDisplayModeIndex(int index) override;
    void setWorkModeIndex(int index) override;
    void setDigitalZoomChecked(bool checked) override;
    void setAutoZoomChecked(bool checked) override;
    void setCaptureUploadChecked(bool checked) override;
    void setPosResetChecked(bool checked) override;
    bool setVideoFrame(const QString& deviceId, const QImage& frame) override;
    void clearVideoFrame(const QString& deviceId) override;
    void setVideoSelectionEnabled(const QString& deviceId, bool enabled) override;
    void setVideoStatusText(const QString& deviceId, const QString& text) override;
    void repaintVideoGrid() override;
    void mapClearAllTracks() override;
    void mapUpdateTargetMarkers(const QJsonArray& targets) override;
    void mapClearFov() override;
    void mapAppendTrackPoint(const QString& trackId, double lat, double lon, double speed) override;
    void mapSetDevicePosition(double lat, double lon) override;
    void mapSetVisFov(double lat, double lon, double panDeg, double tiltDeg,
                      double hfov, double vfov, double distance) override;
    void mapSetIrFov(double lat, double lon, double panDeg, double tiltDeg,
                     double hfov, double vfov, double distance) override;
    void mapSetDeviceInfo(double lat, double lon, double alt, double pan, double tilt,
                          double visHfov, double visVfov, double range, bool rangeEstimated) override;
    bool requireConnected() override;
    bool requireMotorReady() override;

private slots:
    // ── 系统托盘 ──
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShow();
    void onTrayExit();

public slots:
    // 以下槽由 MainPresenter 通过事件总线回调触发（同时实现 IMainView 接口）
    void onDeviceConnected(const QString& deviceId) override;               // 设备连接成功回调
    void onDeviceDisconnected(const QString& deviceId) override;            // 设备断开回调
    void onErrorOccurred(const QString& errorMsg) override;  // 连接错误处理
    void onDeviceReconnecting(int attempt, int maxRetries) override;
    void onDeviceReconnectFailed() override;         // T-JSON ACK 应答处理
    void onRtspStats(const QString& deviceId, const RtspThread::Stats& stats) override;  // RTSP 链路健康

    // ── JSON 数据与抓拍 ──
    void onImageSnapped(const QByteArray& jpegData,     // 抓拍图像回调
                        const QRect& location) override;

    // ── 工作模式切换 ──
    void on_comboWorkMode_currentIndexChanged(int index);

    // ── 标题栏按钮 ──
    void on_btnMenu_Min_clicked();
    void on_btnMenu_Max_clicked();
    void on_btnMenu_Close_clicked();

    // ── 导航栏页面切换 ──
    void on_btnNavMonitor_clicked();
    void on_btnNavPlayback_clicked();
    void on_btnNavLog_clicked();
    void on_btnNavSettings_clicked();

    // ── 界面交互 ──
    void on_btnPtzMoveTo_clicked();                     // 云台转动到指定角度
    void on_btnPtzMoveToGps_clicked();                  // 云台转动到指定经纬度高度
    void on_btnPanZeroCalib_clicked();                  // 水平零点标定
    void on_comboAlgoModel1_currentIndexChanged(int index);
    void on_comboAlgoModel2_currentIndexChanged(int index);
    void on_comboDisplayMode_currentIndexChanged(int index); // 显示模式切换
    void on_btnSetLocation_clicked();                   // 手动下发经纬度
    void on_btnGetImageParams_clicked();                // 查询图像参数

public slots:
    void onRtspOpened(const QString& deviceId) override;                    // RTSP 连接成功
    void onRtspError(const QString& deviceId, const QString &msg) override;   // RTSP 连接出错
    void onVideoSelection(const QString& deviceId, int cx, int cy, int pw, int ph); // 视频画面框选

private:
    Ui::MainWindow *ui;
    VideoGridWidget *m_videoGrid = nullptr;
    DeviceTreeWidget *m_deviceTree = nullptr;
    MapWidget *m_mapWidget = nullptr;
    QWidget *m_mapContainer = nullptr;
    QWidget *m_mapOverlay = nullptr;
    QDialog *m_pipDialog = nullptr;
    QWidget *m_pipTitle = nullptr;
    CmdLogDialog *m_logDialog = nullptr;

                static void refreshStyle(QWidget *w);
private:             // UI 设计器生成的界面对象
        ConfigManager *m_cfg;           // 配置管理器（持久化设置）
            MainPresenter *m_presenter;

    // ── 服务对象 ──
    MainWindowNavigation *m_navigation = nullptr;
    MainWindowDialogService *m_dialogService = nullptr;
    MainWindowLayoutService *m_layoutService = nullptr;
    MainWindowSystemService *m_systemService = nullptr;
    MainWindowControlService *m_controlService = nullptr;

    // ── 私有工具方法 ──
    void setupUiStyles();                           // 加载并应用 QSS 样式表
    void refreshDeviceLabelsAndActive();             // sync device number/name to video badges
    void setupInputValidators();                    // 为数值输入框安装 QValidator，拦截非法字符
};

#endif // MAINWINDOW_H
