#include "MainWindowNavigation.h"
#include "ui/views/cmdlogdialog.h"
#include "ui/main/MainPresenter.h"
#include "ui/main/IMainView.h"
#include <QToolButton>
#include <QPushButton>

MainWindowNavigation::MainWindowNavigation(QObject* parent)
    : QObject(parent)
{
}

void MainWindowNavigation::setup(NavButtons buttons, ConfigManager* cfg, MainPresenter* presenter, IMainView* view)
{
    m_btns = buttons;
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
    // 系统参数设置弹窗已移除，相机/PTZ/镜头参数改为每设备在"设备属性"中编辑
}
