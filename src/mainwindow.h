//============================================================================
// mainwindow.h - T-JSON 主窗口头文件
// 定义主界面类 MainWindow，负责整体 UI 布局、事件响应、设备交互调度
// 以及视频显示、地图展示、云台控制、AI 识别/跟踪结果展示等功能
//============================================================================
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonObject>
#include <QPushButton>
#include <QDateTime>
#include <QTimer>
#include <QVector>
#include <QDialog>
#include "tjsonclient.h"
#include "devicecontroller.h"
#include "configmanager.h"

class RtspThread;
class VideoWidget;
class MapWidget;

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
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // ── 设备连接相关 ──
    void on_btnConnect_clicked();           // 连接/断开设备按钮
    void on_btnCancelConnect_clicked();     // 取消正在进行的连接
    void onDeviceConnected();               // 设备连接成功回调
    void onDeviceDisconnected();            // 设备断开回调
    void onErrorOccurred(const QString& errorMsg);  // 连接错误处理
    void onAckReceived(quint8 statusCode);  // T-JSON ACK 应答处理

    // ── JSON 数据与抓拍 ──
    void onJsonReceived(const QJsonObject& doc);        // 收到设备 JSON 帧
    void onImageSnapped(const QByteArray& jpegData,     // 抓拍图像回调
                        const QRect& location);

    // ── 工作模式切换 ──
    void on_radioModeOff_clicked();          // 关闭 AI
    void on_radioModeIdentify_clicked();     // 识别模式
    void on_radioModeAutoTrack_clicked();    // 自动跟踪模式

    // ── 界面交互 ──
    void on_btnPtzMoveTo_clicked();                     // 云台转动到指定角度
    void on_btnSettings_clicked();                      // 打开设置对话框
    void on_comboAlgoModel_currentIndexChanged(int index); // 算法模型切换
    void on_comboDisplayMode_currentIndexChanged(int index); // 显示模式切换
    void on_comboLensTarget_currentIndexChanged(int index);  // 镜头目标切换
    void on_btnSetLocation_clicked();                   // 手动下发经纬度
    void on_btnGetImageParams_clicked();                // 查询图像参数
    void onSysParamTimerTimeout();                      // 200ms 周期查询系统参数

    // ── RTSP 视频流 ──
    void on_btnVideoConnect_clicked();      // 连接 RTSP 视频流
    void on_btnVideoDisconnect_clicked();   // 断开 RTSP 视频流
    void onRtspFrame(const QImage &frame);  // 收到一帧视频图像
    void onRtspOpened();                    // RTSP 连接成功
    void onRtspError(const QString &msg);   // RTSP 连接出错
    void onVideoSelection(int cx, int cy, int pw, int ph); // 视频画面框选

private:
    Ui::MainWindow *ui;             // UI 设计器生成的界面对象
    TJsonClient *m_client;          // TCP JSON 协议客户端
    ConfigManager *m_cfg;           // 配置管理器（持久化设置）
    DeviceController *m_device;     // 设备指令控制器（封装协议细节）
    RtspThread *m_rtsp;            // RTSP 视频流拉取线程
    MapWidget *m_mapWidget;          // 地图控件（单实例，迷你/全屏切换，含内建工具栏）
    QWidget *m_mapContainer;         // 地图容器（用于拖拽定位）
    QWidget *m_mapOverlay;           // 透明覆盖层（迷你模式拦截鼠标事件）

    bool m_mapVisible = false;       // 地图显示/隐藏
    bool m_mapExpanded = false;      // 迷你/全屏模式
    QPoint m_miniMapPos{10, 10};    // 迷你地图位置
    bool m_dragging = false;         // 拖拽中标记
    QPoint m_dragStart;              // 拖拽起点
    bool m_updatingFromDevice;     // 防递归更新标志，避免设备回传时重复触发 UI 信号

    // ── PiP 视频窗口（大地图时独立无边框对话框） ──
    QDialog *m_pipDialog;
    QWidget *m_pipTitle;
    QPoint m_pipPos{10, 10};
    QPoint m_pipDragStart;

    double m_currentVisZoom;        // 当前可见光镜头倍率（从设备 ZoomInfo 更新）
    double m_currentIrZoom;         // 当前红外镜头倍率
    int m_currentPipShow;           // 当前画中画显示模式（0~4 对应不同布局）
    int m_currentResX = 2688;       // 当前可见光实际水平分辨率（从设备 ImageSize 更新）
    int m_currentResY = 1520;       // 当前可见光实际垂直分辨率
    
    // ── 跟踪状态管理（地图目标/轨迹逻辑） ──
    struct TrackState {
        QString id;             // 当前跟踪目标 ID
        double lat = 0, lon = 0; // 最后已知位置
        int cls = 0;            // 最后 Class
        QDateTime lostSince;    // 首次失锁时间（空 = 锁定中）
        double prevLat = 0, prevLon = 0; // 上一个轨迹点位置（速度计算用）
        QDateTime prevTime;     // 上一个轨迹点时间

        // 抽稀状态：记录上次实际绘制到地图的点
        double plotLat = 0, plotLon = 0;    // 上次绘制点 GPS
        double plotHeading = -1;            // 上次绘制段航向角（度），<0 = 未初始化
        QDateTime plotTime;                 // 上次绘制时间（心跳用）
    };
    TrackState m_track;

    // ── AI 目标距离缓存（用于 ZoomInfo 无激光测距时回退显示） ──
    double m_lastAiDist = 0;            // 最近一次 AIInfo 目标距离（估算或激光）
    bool m_lastAiDistEstimated = false; // true 表示该距离来自视觉估算

    // ── 系统参数轮询（200ms 周期查询设备 ImageSetting） ──
    QTimer *m_sysParamTimer;

    // ── 迷你地图控制 ──
    void toggleMap();                               // 切换地图显示/隐藏
    void toggleMapMode();                           // 切换迷你/全屏模式
    void updateMapLayout();                         // 更新地图尺寸和位置

    // ── 私有工具方法 ──
    bool requireConnected();                        // 未连接时弹出状态栏提示并返回 false
    void setupUiStyles();                           // 加载并应用 QSS 样式表
    void updateStatusFromJson(const QJsonObject& doc); // 解析 JSON 帧并更新所有 UI
    void syncLensTargetByDisplayMode(int pipShow);  // 根据显示模式同步镜头目标选择
    void updateLensStats();                         // 更新镜头统计数据（焦距/视场角）
    QString missMradStr(double dx, double dy,       // 计算脱靶量（毫弧度）
                        double pixelSizeUm, double focalMm);

    // ── 地图辅助方法 ──
    void updateMapDevicePosition(const QJsonObject& doc);    // 更新设备在地图上的位置
    void updateMapTargets(const QJsonObject& doc, int workMode); // 更新地图上的目标标记
    void pixelToGps(double pixelX, double pixelY, double distance,     // 像素坐标 → GPS 坐标
                    double& outLat, double& outLon);
    void pixelBboxToGps(double pixelX, double pixelY, double distance, // 像素框四角 → GPS（含俯仰校正）
                        double tiltDeg, double& outLat, double& outLon);
    double estimateTargetDistance(int boxPixels, double focalMm, double pixelSizeUm, double refSize);
    double calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel);
};

#endif // MAINWINDOW_H