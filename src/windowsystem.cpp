#include "windowsystem.h"
#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QMainWindow>
#include <QEvent>
#include <QMouseEvent>
#include <QIcon>
#include <QPixmap>
#include <QButtonGroup>
#include <QFile>
#include <QApplication>
#include <QStyle>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

WindowSystem::WindowSystem(QWidget *titleBar,
                           QPushButton *btnMin, QPushButton *btnMax, QPushButton *btnClose,
                           QPushButton *btnNavMon, QPushButton *btnNavPb,
                           QPushButton *btnNavLog, QPushButton *btnNavSet,
                           QStackedWidget *contentStack,
                           QLabel *appIcon, QLabel *appTitle,
                           QMainWindow *mainWindow,
                           QObject *parent)
    : QObject(parent)
    , m_titleBar(titleBar)
    , m_btnMin(btnMin), m_btnMax(btnMax), m_btnClose(btnClose)
    , m_btnNavMonitor(btnNavMon), m_btnNavPlayback(btnNavPb)
    , m_btnNavLog(btnNavLog), m_btnNavSettings(btnNavSet)
    , m_contentStack(contentStack)
    , m_appIcon(appIcon), m_appTitle(appTitle)
    , m_mainWindow(mainWindow)
{
    m_titleBar->installEventFilter(this);
    m_titleBar->setProperty("form", "title");

    m_appIcon->setPixmap(QPixmap(QStringLiteral(":/qss/logo.png"))
                         .scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_btnMin->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/minimize.png")));
    m_btnMax->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/maximize.png")));
    m_btnMax->setToolTip(QStringLiteral("最大化"));
    m_btnClose->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/close.png")));
    m_btnClose->setToolTip(QStringLiteral("关闭"));
    m_btnMin->setToolTip(QStringLiteral("最小化"));
    for (auto *b : {m_btnMin, m_btnMax, m_btnClose})
        b->setIconSize(QSize(22, 22));

    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(m_btnNavMonitor, 0);
    navGroup->addButton(m_btnNavPlayback, 1);
    navGroup->addButton(m_btnNavLog, 2);
    navGroup->addButton(m_btnNavSettings, 3);
    m_btnNavMonitor->setChecked(true);

    loadStyles();

    connect(m_btnMin, &QPushButton::clicked, this, &WindowSystem::onMinClicked);
    connect(m_btnMax, &QPushButton::clicked, this, &WindowSystem::onMaxClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &WindowSystem::onCloseClicked);
    connect(m_btnNavMonitor, &QPushButton::clicked, this, &WindowSystem::onNavMonitorClicked);
    connect(m_btnNavPlayback, &QPushButton::clicked, this, &WindowSystem::onNavPlaybackClicked);
    connect(m_btnNavLog, &QPushButton::clicked, this, &WindowSystem::onNavLogClicked);
    connect(m_btnNavSettings, &QPushButton::clicked, this, &WindowSystem::onNavSettingsClicked);
}

void WindowSystem::loadStyles()
{
    QFile file(QStringLiteral(":/style.qss"));
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void WindowSystem::onMinClicked()
{
    m_mainWindow->showMinimized();
}

void WindowSystem::onMaxClicked()
{
    if (m_mainWindow->isMaximized())
        m_mainWindow->showNormal();
    else
        m_mainWindow->showMaximized();
}

void WindowSystem::onCloseClicked()
{
    // 直接退出应用，确保 CmdLogDialog 等所有子窗口关闭
    QApplication::quit();
}

void WindowSystem::onNavMonitorClicked()   { m_contentStack->setCurrentIndex(0); }
void WindowSystem::onNavPlaybackClicked()  { m_contentStack->setCurrentIndex(1); }
void WindowSystem::onNavLogClicked()       { m_contentStack->setCurrentIndex(2); }
void WindowSystem::onNavSettingsClicked()  { m_contentStack->setCurrentIndex(3); }

bool WindowSystem::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_titleBar) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                QWidget *child = m_titleBar->childAt(me->pos());
                if (child == m_btnMin || child == m_btnMax || child == m_btnClose
                    || child == m_btnNavMonitor || child == m_btnNavPlayback
                    || child == m_btnNavLog || child == m_btnNavSettings)
                    return false;
                m_windowDragPos = me->globalPosition().toPoint();
                m_windowDragging = true;
            }
            return true;
        }
        case QEvent::MouseMove: {
            if (m_windowDragging) {
                auto *me = static_cast<QMouseEvent*>(event);
                QPoint delta = me->globalPosition().toPoint() - m_windowDragPos;
                m_mainWindow->move(m_mainWindow->pos() + delta);
                m_windowDragPos = me->globalPosition().toPoint();
            }
            return true;
        }
        case QEvent::MouseButtonRelease: {
            m_windowDragging = false;
            return true;
        }
        case QEvent::MouseButtonDblClick: {
            onMaxClicked();
            return true;
        }
        default:
            break;
        }
    }
    return QObject::eventFilter(obj, event);
}

void WindowSystem::handleChangeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        bool isMax = m_mainWindow->isMaximized();
        m_btnMax->setIcon(QIcon(isMax
            ? QStringLiteral(":/qss/blacksoft/restore.png")
            : QStringLiteral(":/qss/blacksoft/maximize.png")));
        m_btnMax->setToolTip(isMax ? QStringLiteral("向下还原") : QStringLiteral("最大化"));
    }
}

#ifdef Q_OS_WIN
static bool isChildOf(const QWidget *parent, const QWidget *child)
{
    if (!parent || !child) return false;
    while (child) {
        if (child == parent) return true;
        child = child->parentWidget();
    }
    return false;
}

static bool childAtWidgetPos(const QWidget *parent, const QPoint &local, const QWidget *target)
{
    if (!parent) return false;
    QWidget *c = parent->childAt(local);
    return c == target || isChildOf(c, target);
}
#endif

bool WindowSystem::handleNativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        switch (msg->message) {

        case WM_NCCALCSIZE:
            *result = 0;
            return true;

        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
            QPoint local = m_mainWindow->mapFromGlobal(QPoint(pt.x, pt.y));

            if (!m_mainWindow->isMaximized()) {
                int b = 8;
                if (local.y() <= b && local.x() <= b) { *result = HTTOPLEFT; return true; }
                if (local.y() <= b && local.x() >= m_mainWindow->width() - b) { *result = HTTOPRIGHT; return true; }
                if (local.y() >= m_mainWindow->height() - b && local.x() <= b) { *result = HTBOTTOMLEFT; return true; }
                if (local.y() >= m_mainWindow->height() - b && local.x() >= m_mainWindow->width() - b) { *result = HTBOTTOMRIGHT; return true; }
                if (local.y() <= b) { *result = HTTOP; return true; }
                if (local.y() >= m_mainWindow->height() - b) { *result = HTBOTTOM; return true; }
                if (local.x() <= b) { *result = HTLEFT; return true; }
                if (local.x() >= m_mainWindow->width() - b) { *result = HTRIGHT; return true; }
            }

            if (m_titleBar && m_titleBar->isVisible()) {
                QPoint tl = m_titleBar->mapFromGlobal(QPoint(pt.x, pt.y));
                if (m_titleBar->rect().contains(tl)) {
                    QWidget *child = m_titleBar->childAt(tl);
                    if (child == m_btnMin || child == m_btnMax || child == m_btnClose
                        || child == m_btnNavMonitor || child == m_btnNavPlayback
                        || child == m_btnNavLog || child == m_btnNavSettings)
                        { *result = HTCLIENT; return true; }
                    *result = HTCAPTION;
                    return true;
                }
            }
            return false;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            RECT wa;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
            mmi->ptMaxPosition.x = wa.left;
            mmi->ptMaxPosition.y = wa.top;
            mmi->ptMaxSize.x = wa.right - wa.left;
            mmi->ptMaxSize.y = wa.bottom - wa.top;
            return true;
        }

        case WM_NCACTIVATE:
            *result = 1;
            return true;
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}
