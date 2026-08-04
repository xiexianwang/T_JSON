#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class DeviceManager;
class TrackManager;
class MapViewController;
class ConfigManager;
class WindowSystem;
class DeviceInteractionController;
struct DeviceState;

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void resizeEvent(QResizeEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void changeEvent(QEvent *event) override;

private:
    Ui::MainWindow *ui = nullptr;
    ConfigManager *m_cfg = nullptr;
    DeviceManager *m_devMgr = nullptr;
    TrackManager *m_trackMgr = nullptr;
    MapViewController *m_mapCtrl = nullptr;
    DeviceState *m_devState = nullptr;
    WindowSystem *m_windowSystem = nullptr;
    DeviceInteractionController *m_interactionCtrl = nullptr;
};

#endif // MAINWINDOW_H
