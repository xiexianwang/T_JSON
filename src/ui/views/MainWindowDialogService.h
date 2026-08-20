#ifndef MAINWINDOWDIALOGSERVICE_H
#define MAINWINDOWDIALOGSERVICE_H

#include <QObject>

class QDialog;
class QSystemTrayIcon;
class QMenu;
class CmdLogDialog;
class ConfigManager;
class MainPresenter;
class IMainView;

class MainWindowDialogService : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowDialogService(QObject* parent = nullptr);

    void setup(ConfigManager* cfg, MainPresenter* presenter, IMainView* view);
    void setLogDialog(CmdLogDialog* dlg);
    void setupSystemTray(QSystemTrayIcon* trayIcon, QMenu* trayMenu);

    // 对话框操作
    bool showCloseConfirmation();
    void showAbout();
    bool requireConnected();
    bool requireMotorReady();

signals:
    void trayShowRequested();
    void trayExitRequested();

private:
    ConfigManager* m_cfg = nullptr;
    MainPresenter* m_presenter = nullptr;
    IMainView* m_view = nullptr;
    CmdLogDialog* m_logDialog = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;
};

#endif // MAINWINDOWDIALOGSERVICE_H
