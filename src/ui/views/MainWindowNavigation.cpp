#include "MainWindowNavigation.h"
#include "ui/views/settingsdialog.h"
#include "ui/views/cmdlogdialog.h"
#include "ui/main/MainPresenter.h"
#include "ui/main/IMainView.h"
#include <QToolButton>
#include <QPushButton>

MainWindowNavigation::MainWindowNavigation(QObject* parent)
    : QObject(parent)
{
}

void MainWindowNavigation::setup(NavButtons buttons, QPushButton* btnMapToggle,
                                  ConfigManager* cfg, MainPresenter* presenter, IMainView* view)
{
    m_btns = buttons;
    m_btnMapToggle = btnMapToggle;
    m_cfg = cfg;
    m_presenter = presenter;
    m_view = view;
}

void MainWindowNavigation::setLogDialog(CmdLogDialog* dlg)
{
    m_logDialog = dlg;
}

void MainWindowNavigation::onBtnNavMonitorClicked()
{
    // 当前页面，无需操作
}

void MainWindowNavigation::onBtnNavPlaybackClicked()
{
    // 预留
}

void MainWindowNavigation::onBtnNavLogClicked()
{
    if (!m_logDialog) return;
    if (m_logDialog->isVisible()) {
        m_logDialog->hide();
    } else {
        m_logDialog->show();
        m_logDialog->raise();
        m_logDialog->activateWindow();
    }
}

void MainWindowNavigation::onBtnNavSettingsClicked()
{
    SettingsDialog dlg(m_cfg, m_view->asWidget());
    dlg.exec();

    // 电机协议变更后重新打开串口
    m_presenter->applyMotorChannel();
    // 重启 PTZ 转发服务
    m_presenter->initPtzForwarder();
}
