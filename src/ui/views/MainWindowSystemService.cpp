#include "MainWindowSystemService.h"
#include "infrastructure/configmanager.h"
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QSystemTrayIcon>

MainWindowSystemService::MainWindowSystemService(QObject* parent)
    : QObject(parent)
{
}

void MainWindowSystemService::setup(const Setup& setup)
{
    m_setup = setup;
    m_setup.trayMenu->addAction(QStringLiteral("显示主窗口"), this, &MainWindowSystemService::onTrayShow);
    m_setup.trayMenu->addSeparator();
    m_setup.trayMenu->addAction(QStringLiteral("退出"), this, &MainWindowSystemService::onTrayExit);
    m_setup.trayIcon->setContextMenu(m_setup.trayMenu);
    connect(m_setup.trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindowSystemService::onTrayActivated);
    m_setup.trayIcon->show();
}

void MainWindowSystemService::onMinimize()
{
    m_setup.showMinimized();
}

void MainWindowSystemService::onMaximize()
{
    m_setup.toggleMaximized();
}

void MainWindowSystemService::onClose()
{
    if (m_setup.closeConfirmed)
        m_setup.closeConfirmed();
}

void MainWindowSystemService::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        onTrayShow();
}

void MainWindowSystemService::onTrayShow()
{
    m_setup.showNormalAndActivate();
}

void MainWindowSystemService::onTrayExit()
{
    m_setup.trayIcon->hide();
    if (m_setup.trayExit)
        m_setup.trayExit();
    qApp->quit();
}

void MainWindowSystemService::handleCloseEvent(QCloseEvent* event)
{
    const auto action = m_setup.config->closeAction();
    if (action == ConfigManager::Exit) {
        m_setup.trayIcon->hide();
        qApp->quit();
        event->accept();
    } else if (action == ConfigManager::Minimize) {
        m_setup.hideWindow();
        event->ignore();
    } else if (m_setup.trayIcon->isVisible()) {
        m_setup.hideWindow();
        m_setup.trayIcon->showMessage(QStringLiteral("LSS Video Manager"),
            QStringLiteral("程序已最小化到系统托盘"), QSystemTrayIcon::Information, 2000);
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindowSystemService::handleChangeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        const bool max = m_setup.isMaximized();
        m_setup.maximize->setIcon(QIcon(max
            ? QStringLiteral(":/qss/blacksoft/restore.png")
            : QStringLiteral(":/qss/blacksoft/maximize.png")));
        m_setup.maximize->setToolTip(max
            ? QString::fromUtf8("窗口化") : QString::fromUtf8("最大化"));
    }
}
