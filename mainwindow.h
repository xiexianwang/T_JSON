#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonObject>
#include "tjsonclient.h"
#include "devicecontroller.h"

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

private:
    Ui::MainWindow *ui;
    TJsonClient *m_client;
    DeviceController *m_device;
    
    void setupUiStyles();
    void updateStatusFromJson(const QJsonObject& doc);
};

#endif // MAINWINDOW_H