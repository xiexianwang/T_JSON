#ifndef WINDOWSYSTEM_H
#define WINDOWSYSTEM_H

#include <QObject>
#include <QPoint>

class QWidget;
class QPushButton;
class QToolButton;
class QStackedWidget;
class QLabel;
class QEvent;
class QMainWindow;

class WindowSystem : public QObject
{
    Q_OBJECT
public:
    WindowSystem(QWidget *titleBar,
                 QPushButton *btnMin, QPushButton *btnMax, QPushButton *btnClose,
                 QToolButton *btnNavMon, QToolButton *btnNavPb,
                 QToolButton *btnNavLog, QToolButton *btnNavSet,
                 QStackedWidget *contentStack,
                 QLabel *appIcon, QLabel *appTitle,
                 QMainWindow *mainWindow,
                 QObject *parent = nullptr);

    bool handleNativeEvent(const QByteArray &eventType, void *message, qintptr *result);
    void handleChangeEvent(QEvent *event);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void loadStyles();
    void onMinClicked();
    void onMaxClicked();
    void onCloseClicked();
    void onNavMonitorClicked();
    void onNavPlaybackClicked();
    void onNavLogClicked();
    void onNavSettingsClicked();

    QWidget *m_titleBar;
    QPushButton *m_btnMin, *m_btnMax, *m_btnClose;
    QToolButton *m_btnNavMonitor, *m_btnNavPlayback, *m_btnNavLog, *m_btnNavSettings;
    QStackedWidget *m_contentStack;
    QLabel *m_appIcon, *m_appTitle;
    QMainWindow *m_mainWindow;
    bool m_windowDragging = false;
    QPoint m_windowDragPos;
};

#endif // WINDOWSYSTEM_H
