#ifndef DEVICEINTERACTIONCONTROLLER_H
#define DEVICEINTERACTIONCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QString>

class QTimer;
class QJsonObject;
class QImage;
class QRect;
class VideoGridWidget;
class QWidget;
class DeviceTreeWidget;
class QPushButton;
class QButtonGroup;
class DeviceManager;
class TrackManager;
class MapViewController;
class ConfigManager;
class PtzForwarder;
struct DeviceState;
class QStatusBar;
struct CameraConfig;

namespace Ui {
class MainWindow;
}

class DeviceInteractionController : public QObject
{
    Q_OBJECT
public:
    DeviceInteractionController(Ui::MainWindow *ui,
                                VideoGridWidget *videoGrid,
                                QWidget *drawerPanel,
                                DeviceTreeWidget *deviceTree,
                                QPushButton *drawerToggleBtn,
                                QWidget *mainWindow,
                                DeviceManager *devMgr,
                                TrackManager *trackMgr,
                                MapViewController *mapCtrl,
                                ConfigManager *cfg,
                                DeviceState *devState,
                                PtzForwarder *ptzForwarder,
                                QStatusBar *statusBar,
                                QObject *parent = nullptr);

    void handleResize();

private:
    // ── JSON 帧接收与分发 ──
    void onDeviceJsonReceived(const QString &ip, const QJsonObject &doc);
    void onSysParamTimerTimeout();

    // ── 工作模式切换 ──
    void onRadioModeOffClicked();
    void onRadioModeIdentifyClicked();
    void onRadioModeAutoTrackClicked();

    // ── 界面交互 ──
    void onPtzMoveToClicked();
    void onPtzMoveToGpsClicked();
    void onPanZeroCalibClicked();

    // ── 雨刷电机与MODBUS ──
    void onWiperStartClicked();
    void onWiperStopClicked();
    void onWiperLeftPressed();
    void onWiperLeftReleased();
    void onWiperRightPressed();
    void onWiperRightReleased();
    void onWiperZeroCalibClicked();
    void onWiperModeClicked();
    void onWiperSilentClicked();
    void onWiperCurrentEditingFinished();

    void onSettingsClicked();
    void onAlgoModelChanged(int index);
    void onDisplayModeChanged(int index);
    void onLensTargetChanged(int index);
    void onSetLocationClicked();
    void onGetImageParamsClicked();

    // ── 设备树与分屏 ──
    void onDeviceTreeDoubleClicked(const QString &name, const QString &ip, const QString &rtspUrl);
    void onGridCellSelected(int cellIndex);
    void onDrawerToggled();
    void switchActiveDevice(const QString &ip);
    void onVideoSelection(int cx, int cy, int pw, int ph);
    void onImageSnapped(const QByteArray &jpegData, const QRect &location);

    // ── 辅助方法 ──
    bool requireConnected();
    void updateStatusFromJson(const QJsonObject &doc);
    void updateLensStats();
    void syncLensTargetByDisplayMode(int pipShow);

    Ui::MainWindow *ui;
    QWidget *m_mainWindow;
    DeviceManager *m_devMgr;
    TrackManager *m_trackMgr;
    MapViewController *m_mapCtrl;
    ConfigManager *m_cfg;
    DeviceState *m_devState;
    QStatusBar *m_statusBar;
    PtzForwarder *m_ptzForwarder;

    // ── 视频网格与抽屉 ──
    VideoGridWidget *m_videoGrid;
    QWidget *m_drawerPanel;
    DeviceTreeWidget *m_deviceTree;
    QPushButton *m_drawerToggleBtn;
    QButtonGroup *m_splitGroup;
    bool m_drawerVisible = true;
    QMap<int, QString> m_gridCellMap;

    // ── 系统参数轮询 ──
    QTimer *m_sysParamTimer;
};

#endif // DEVICEINTERACTIONCONTROLLER_H
