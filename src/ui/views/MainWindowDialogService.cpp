#include "MainWindowDialogService.h"
#include "ui/views/cmdlogdialog.h"
#include "ui/main/MainPresenter.h"
#include "ui/main/IMainView.h"
#include "infrastructure/configmanager.h"
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QCheckBox>
#include <QDialog>
#include <QPushButton>

MainWindowDialogService::MainWindowDialogService(QObject* parent)
    : QObject(parent)
{
}

void MainWindowDialogService::setup(ConfigManager* cfg, MainPresenter* presenter, IMainView* view)
{
    m_cfg = cfg;
    m_presenter = presenter;
    m_view = view;
}

void MainWindowDialogService::setLogDialog(CmdLogDialog* dlg)
{
    m_logDialog = dlg;
}

void MainWindowDialogService::setupSystemTray(QSystemTrayIcon* trayIcon, QMenu* trayMenu)
{
    m_trayIcon = trayIcon;

    connect(trayMenu->actions().at(0), &QAction::triggered, this, &MainWindowDialogService::trayShowRequested);
    connect(trayMenu->actions().at(2), &QAction::triggered, this, &MainWindowDialogService::trayExitRequested);
}

bool MainWindowDialogService::showCloseConfirmation()
{
    auto action = m_cfg->closeAction();
    if (action == ConfigManager::Exit) {
        return true;  // 直接退出
    }
    if (action == ConfigManager::Minimize) {
        m_view->asWidget()->hide();
        return false;  // 最小化，不退出
    }

    // 弹出选择对话框
    QDialog dlg(m_view->asWidget());
    dlg.setWindowTitle(QStringLiteral("关闭提示"));
    dlg.setFixedSize(300, 160);
    dlg.setWindowFlags((dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint));

    auto *layout = new QVBoxLayout(&dlg);

    auto *radioLayout = new QHBoxLayout();
    auto *radioExit = new QRadioButton(QStringLiteral("退出程序"), &dlg);
    auto *radioMin = new QRadioButton(QStringLiteral("最小化到托盘"), &dlg);
    radioMin->setChecked(true);
    radioLayout->addWidget(radioExit);
    radioLayout->addStretch();
    radioLayout->addWidget(radioMin);
    layout->addLayout(radioLayout);

    auto *bottomLayout = new QHBoxLayout();
    auto *cbRemember = new QCheckBox(QStringLiteral("记住本次选择"), &dlg);
    bottomLayout->addWidget(cbRemember);
    bottomLayout->addStretch();
    auto *btnConfirm = new QPushButton(QStringLiteral("确认"), &dlg);
    btnConfirm->setFixedWidth(80);
    bottomLayout->addWidget(btnConfirm);
    layout->addLayout(bottomLayout);

    bool shouldExit = false;
    connect(btnConfirm, &QPushButton::clicked, this, [&]() {
        if (cbRemember->isChecked()) {
            m_cfg->setCloseAction(radioExit->isChecked()
                ? ConfigManager::Exit : ConfigManager::Minimize);
            m_cfg->save();
        }
        shouldExit = radioExit->isChecked();
        dlg.close();
    });

    dlg.exec();
    if (!shouldExit)          // 选了最小化（无论是否记住）→ 隐藏到托盘
        m_view->asWidget()->hide();
    return shouldExit;
}

void MainWindowDialogService::showAbout()
{
    QMessageBox::about(m_view->asWidget(),
        QStringLiteral("关于 LSS Video Manager"),
        QStringLiteral("LSS Video Manager v2.1\n江苏莱瑟斯监控设备控制客户端"));
}

bool MainWindowDialogService::requireConnected()
{
    if (!m_presenter->isDeviceConnected()) {
        QMessageBox msgBox(m_view->asWidget());
        msgBox.setWindowTitle(QStringLiteral("提示"));
        msgBox.setText(QStringLiteral("请连接设备"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
        msgBox.exec();
        return false;
    }
    return true;
}

bool MainWindowDialogService::requireMotorReady()
{
    if (!m_presenter->isDeviceConnected()) {
        QMessageBox msgBox(m_view->asWidget());
        msgBox.setWindowTitle(QStringLiteral("提示"));
        msgBox.setText(QStringLiteral("请先连接设备"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
        msgBox.exec();
        return false;
    }

    // 通道检测按协议/通道区分：
    //   MODBUS-RTU + 串口通道  → 必须已打开串口
    //   STM32-TCP-V4.0        → 必须已建立 TCP
    //   Pelco-D / MODBUS 透传  → 走 Pelco-D 透传，无需检测串口连接
    const QString proto = m_presenter->motorProtocol();
    const QString channel = m_presenter->motorCommandChannel();

    bool channelReady = true;
    QString message;
    if (proto == QStringLiteral("MODBUS-RTU") && channel == QStringLiteral("串口")) {
        channelReady = m_presenter->isMotorSerialOpen();
        message = QStringLiteral("电机串口未打开，请在设备属性中配置");
    } else if (proto == QStringLiteral("STM32-TCP-V4.0")) {
        channelReady = m_presenter->isMotorTcpOpen();
        message = QStringLiteral("电机 TCP 未连接，请在设备属性中配置");
    }

    if (!channelReady) {
        QMessageBox msgBox(m_view->asWidget());
        msgBox.setWindowTitle(QStringLiteral("提示"));
        msgBox.setText(message);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
        msgBox.exec();
        return false;
    }
    return true;
}
