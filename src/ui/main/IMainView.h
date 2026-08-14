#ifndef IMAINVIEW_H
#define IMAINVIEW_H

#include <QString>
#include <QByteArray>
#include <QRect>

class QWidget;
class VideoWidget;
class MapWidget;

// ============================================================================
// IMainView - MainWindow 的窄 View 接口（MVP 解耦边界）
//
// MainPresenter 只依赖此接口，不再直接操作 Ui::MainWindow 控件。
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

    // ---- 连接按钮状态 ----
    virtual void setConnectButton(const QString& text, bool enabled,
                                  const QString& state = QString(),
                                  bool cancelVisible = false) = 0;

    // ---- 视频连接按钮状态 ----
    virtual void setVideoConnectButton(const QString& text, bool enabled) = 0;

    // ---- 输入读取 ----
    virtual QString ipText() const = 0;
    virtual QString rtspUrlText() const = 0;
    virtual QString targetPanText() const = 0;
    virtual QString targetTiltText() const = 0;
    virtual QString targetLonText() const = 0;
    virtual QString targetLatText() const = 0;
    virtual QString targetAltText() const = 0;
    virtual QString setLatText() const = 0;
    virtual QString setLonText() const = 0;
    virtual QString setHeightText() const = 0;
    virtual int wiperCurrentMa() const = 0;
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
    virtual void showDeviceState(int camMode, const QString& lat, const QString& lon,
                                 const QString& height, const QString& pan,
                                 const QString& tilt) = 0;

    // ---- 镜头统计显示 ----
    virtual void showLensStats(double visZoom, double visFocal, double visHfov,
                               double irZoom, double irFocal, double irHfov) = 0;

    // ---- AI 识别表格 ----
    virtual void setIdentifyCount(const QString& text) = 0;
    virtual void clearIdentifyTable() = 0;
    virtual void addIdentifyRow(const QString& id, int cls, double dist,
                                const QString& pos, const QString& miss) = 0;

    // ---- 跟踪状态显示 ----
    virtual void showTrackStatus(const QString& text, const QString& state) = 0;
    virtual void setTrackDistance(const QString& text) = 0;   // 空串 = 清除
    virtual void setTrackPos(const QString& text) = 0;
    virtual void setTrackMissDistance(const QString& text) = 0;

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

    // ---- 视频网格 ----
    virtual VideoWidget* videoWidget(const QString& deviceId) = 0;
    virtual void repaintVideoGrid() = 0;

    // ---- 地图 ----
    virtual MapWidget* mapWidget() = 0;

    // ---- 连接/电机校验（含弹窗提示） ----
    virtual bool requireConnected() = 0;
    virtual bool requireMotorReady() = 0;

    // ---- 抓拍图像保存 ----
    virtual void onImageSnapped(const QByteArray& jpegData, const QRect& location) = 0;

    // ---- Presenter → View 状态回调 ----
    virtual void onDeviceConnected() = 0;
    virtual void onDeviceDisconnected() = 0;
    virtual void onErrorOccurred(const QString& errorMsg) = 0;
    virtual void onRtspOpened() = 0;
    virtual void onRtspError(const QString& msg) = 0;
    virtual void onDeviceReconnecting(int attempt, int maxRetries) = 0;
    virtual void onDeviceReconnectFailed() = 0;
};

#endif // IMAINVIEW_H
