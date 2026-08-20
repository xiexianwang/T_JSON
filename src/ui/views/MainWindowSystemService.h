#ifndef MAINWINDOWSYSTEMSERVICE_H
#define MAINWINDOWSYSTEMSERVICE_H

#include <QObject>
#include <QSystemTrayIcon>
#include <functional>

class QCloseEvent;
class QEvent;
class QMenu;
class QPushButton;
class ConfigManager;

class MainWindowSystemService : public QObject
{
    Q_OBJECT
public:
    struct Setup {
        QPushButton* minimize = nullptr;
        QPushButton* maximize = nullptr;
        QPushButton* close = nullptr;
        QSystemTrayIcon* trayIcon = nullptr;
        QMenu* trayMenu = nullptr;
        ConfigManager* config = nullptr;
        std::function<void()> closeConfirmed;
        std::function<void()> trayExit;
        std::function<bool()> isMaximized;
        std::function<void()> showMinimized;
        std::function<void()> toggleMaximized;
        std::function<void()> showNormalAndActivate;
        std::function<void()> hideWindow;
        std::function<void()> showWindow;
    };

    explicit MainWindowSystemService(QObject* parent = nullptr);
    void setup(const Setup& setup);
    void onMinimize();
    void onMaximize();
    void onClose();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShow();
    void onTrayExit();
    void handleCloseEvent(QCloseEvent* event);
    void handleChangeEvent(QEvent* event);

private:
    Setup m_setup;
};

#endif // MAINWINDOWSYSTEMSERVICE_H
