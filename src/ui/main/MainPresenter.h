#ifndef MAINPRESENTER_H
#define MAINPRESENTER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QRect>

class MainWindow;
class DeviceContext;
class ConfigManager;
class DeviceController;
class TJsonClient;
class RtspThread;
class PtzForwarder;

// ============================================================================
// MainPresenter - MainWindow 的控制器 (MVP 模式中的 Presenter)
// ============================================================================
class MainPresenter : public QObject
{
    Q_OBJECT
public:
    explicit MainPresenter(MainWindow* view, ConfigManager* cfg, QObject *parent = nullptr);
    ~MainPresenter() override;

    // --- 供 View 调用的命令接口 ---
    void connectToDevice(const QString& ip, quint16 port);
    void disconnectDevice();
    void ptzMove(int direction);
    void ptzStop();

    // --- 提取的业务按钮逻辑 ---
    void on_btnConnect_clicked();
    void on_btnCancelConnect_clicked();
    void on_btnVideoConnect_clicked();
    void on_btnVideoDisconnect_clicked();
    void on_btnPtzMoveTo_clicked();
    void on_btnPtzMoveToGps_clicked();
    void on_btnPanZeroCalib_clicked();
    void on_btnSetLocation_clicked();
    void on_btnGetImageParams_clicked();

    
    // 过渡期接口：为了不一次性引发几百个编译错误，提供底层组件的访问器
    DeviceController* motorController() const;
    TJsonClient* tcpClient() const;
    RtspThread* videoStream() const;
    PtzForwarder* ptzForwarder() const;

private:
    MainWindow* m_view;
    ConfigManager* m_cfg;
    
    QString m_currentDeviceId;

    void setupEventBus();
};

#endif // MAINPRESENTER_H
