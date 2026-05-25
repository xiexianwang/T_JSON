#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonObject>
#include "tjsonclient.h"
#include "devicecontroller.h"
#include "configmanager.h"

class RtspThread;
class VideoWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btnConnect_clicked();
    void on_btnCancelConnect_clicked();
    void on_btnVideoConnect_clicked();
    void on_btnVideoDisconnect_clicked();
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onErrorOccurred(const QString& errorMsg);
    void onJsonReceived(const QJsonObject& doc);
    void onImageSnapped(const QByteArray& jpegData, const QRect& location);

    void on_radioModeOff_clicked();
    void on_radioModeIdentify_clicked();
    void on_radioModeAutoTrack_clicked();
    void on_btnPtzMoveTo_clicked();
    void on_btnSettings_clicked();
    void on_comboAlgoModel_currentIndexChanged(int index);
    void on_comboDisplayMode_currentIndexChanged(int index);
    void on_comboLensTarget_currentIndexChanged(int index);
    void on_btnSetLocation_clicked();
    void on_btnGetImageParams_clicked();

    void onRtspFrame(const QImage &frame);
    void onRtspOpened();
    void onRtspError(const QString &msg);
    void onVideoSelection(const QRectF &normRect);

private:
    Ui::MainWindow *ui;
    TJsonClient *m_client;
    ConfigManager *m_cfg;
    DeviceController *m_device;
    RtspThread *m_rtsp;
    bool m_updatingFromDevice = false;

    double m_currentVisZoom = 1.0;
    double m_currentIrZoom = 1.0;
    int m_currentPipShow = 0;
    
    void setupUiStyles();
    void updateStatusFromJson(const QJsonObject& doc);
    void syncLensTargetByDisplayMode(int pipShow);
    void updateLensStats();
    QString missMradStr(double dx, double dy, double pixelSizeUm, double focalMm);
};

#endif // MAINWINDOW_H