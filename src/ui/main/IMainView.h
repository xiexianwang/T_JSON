#ifndef IMAINVIEW_H
#define IMAINVIEW_H

#include <QString>
#include <QByteArray>
#include <QRect>
#include <QJsonArray>
#include "infrastructure/rtspthread.h"

class QWidget;

// ============================================================================
// IMainView - MainWindow 的窄 View 接口（MVP 解耦边界）
//
// MainPresenter 只依赖此接口，不再直接操作 Ui::MainWindow 控件，
// 也不直接访问 VideoWidget / MapWidget 等具体 Widget 类型。
// MainWindow 实现该接口，负责布局、信号连接与展示逻辑。
// ============================================================================
class IMainView
{
public:
    virtual ~IMainView() = default;

    // ---- 原生控件访问（供 Presenter 弹窗归属父窗口） ----
    virtual QWidget* asWidget() = 0;

    // ---- 状态栏/提示 ----
    virtual void showStatusMessage(const QString& msg, int timeoutMs = 0) = 0;

    // ---- 输入读取 ----
    virtual QString targetPanText() const = 0;
    virtual QString targetTiltText() const = 0;
    virtual QString targetLonText() const = 0;
    virtual QString targetLatText() const = 0;
    virtual QString targetAltText() const = 0;
    virtual QString setLatText() const = 0;
    virtual QString setLonText() const = 0;
    virtual QString setHeightText() const = 0;
    virtual int wiperRunCurrent() const = 0;
    virtual int wiperHoldCurrent() const = 0;
    virtual int wiperHoldDelay() const = 0;
    virtual int presetValue() const = 0;
    virtual int workModeIndex() const = 0;
    virtual int algoModel2Index() const = 0;
    virtual int displayModeIndex() const = 0;

    // ---- 设备状态回读（地图/目标计算用） ----
    virtual QString statLatitudeText() const = 0;
    virtual QString statLongitudeText() const = 0;
    virtual QString statPanAngleText() const = 0;
    virtual QString statTiltAngleText() const = 0;

    // ---- 设备状态显示 ----
    virtual void showDeviceState(const QString& lat, const QString& lon,
                                 const QString& height, const QString& pan,
                                 const QString& tilt) = 0;

    // ---- 镜头统计显示 ----
    virtual void showLensStats(double visZoom, double visFocal, double visHfov,
                               double irZoom, double irFocal, double irHfov) = 0;

    // ---- AI 识别表格 ----
    virtual void setIdentifyCount(const QString& text) = 0;
    virtual void clearIdentifyTable() = 0;
    virtual void addIdentifyRow(const QString& id, const QString& typeName, double dist,
                                const QString& pos, const QString& miss) = 0;

    // ---- 跟踪状态显示 ----
    virtual void showTrackStatus(const QString& text, const QString& state) = 0;
    virtual void setTrackDistance(const QString& text) = 0;   // 空串 = 清除
    virtual void setTrackPos(const QString& text) = 0;
    virtual void setTrackMissDistance(const QString& text) = 0;
    virtual void setTrackTargetType(const QString& text) = 0; // 目标类型（按当前模型）
    virtual void setTrackAngle(const QString& text) = 0;      // 设备上报水平/垂直角度

    // ---- 图像参数显示 ----
    virtual void showImageParams(const QString& resolution, const QString& bitrate,
                                 const QString& codec, const QString& workMode,
                                 const QString& pipShow, const QString& algoModel,
                                 const QString& maxVisFL, const QString& maxIRFL) = 0;

    // ---- 下拉框/复选框同步（实现内部 blockSignals 防递归） ----
    virtual void setAlgoModel1Index(int high) = 0;
    virtual void setAlgoModel2Index(int low) = 0;
    virtual void setDisplayModeIndex(int index) = 0;
    virtual void setWorkModeIndex(int index) = 0;
    virtual void setDigitalZoomChecked(bool checked) = 0;
    virtual void setAutoZoomChecked(bool checked) = 0;
    virtual void setCaptureUploadChecked(bool checked) = 0;
    virtual void setPosResetChecked(bool checked) = 0;

    // ---- 视频网格（Presenter 不再直接操作 VideoWidget） ----
    // 返回值：true = 已成功绑定视频槽位；false = 槽位已满（调用方需提示用户）
    virtual bool setVideoFrame(const QString& deviceId, const QImage& frame) = 0;
    virtual void clearVideoFrame(const QString& deviceId) = 0;
    virtual void setVideoSelectionEnabled(const QString& deviceId, bool enabled) = 0;
    virtual void setVideoStatusText(const QString& deviceId, const QString& text) = 0;
    virtual void repaintVideoGrid() = 0;

    // ---- 地图操作（Presenter 不再直接操作 MapWidget） ----
    virtual void mapClearAllTracks() = 0;
    virtual void mapUpdateTargetMarkers(const QJsonArray& targets) = 0;
    virtual void mapClearFov() = 0;
    virtual void mapAppendTrackPoint(const QString& trackId, double lat, double lon, double speed) = 0;
    virtual void mapSetDevicePosition(double lat, double lon) = 0;
    virtual void mapSetVisFov(double lat, double lon, double panDeg, double tiltDeg,
                              double hfov, double vfov, double distance) = 0;
    virtual void mapSetIrFov(double lat, double lon, double panDeg, double tiltDeg,
                             double hfov, double vfov, double distance) = 0;
    virtual void mapSetDeviceInfo(double lat, double lon, double alt, double pan, double tilt,
                                  double visHfov, double visVfov, double range, bool rangeEstimated) = 0;

    // ---- 连接/电机校验（含弹窗提示） ----
    virtual bool requireConnected() = 0;
    virtual bool requireMotorReady() = 0;

    // ---- 抓拍图像保存 ----
    virtual void onImageSnapped(const QByteArray& jpegData, const QRect& location) = 0;

    // ---- Presenter → View 状态回调 ----
    virtual void onDeviceConnected(const QString& deviceId) = 0;
    virtual void onDeviceDisconnected(const QString& deviceId) = 0;
    virtual void onErrorOccurred(const QString& errorMsg) = 0;
    virtual void onRtspOpened(const QString& deviceId) = 0;
    virtual void onRtspError(const QString& deviceId, const QString& msg) = 0;
    virtual void onDeviceReconnecting(int attempt, int maxRetries) = 0;
    virtual void onDeviceReconnectFailed() = 0;

    // ---- RTSP 链路健康（可观测性） ----
    virtual void onRtspStats(const QString& deviceId, const RtspThread::Stats& stats) = 0;
};

#endif // IMAINVIEW_H
