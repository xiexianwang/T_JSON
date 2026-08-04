#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "devicemanager.h"
#include "trackmanager.h"
#include "mapviewcontroller.h"
#include "configmanager.h"
#include "devicestate.h"
#include "windowsystem.h"
#include "deviceinteractioncontroller.h"
#include "videogridwidget.h"
#include "devicetreewidget.h"
#include "cmdlogdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_cfg(new ConfigManager(this))
    , m_trackMgr(new TrackManager(this))
    , m_mapCtrl(nullptr)
{
    ui->setupUi(this);

    // 共享设备状态
    m_devState = new DeviceState;

    // ── 视频网格 + 抽屉 + 设备树 ──
    auto *videoGrid = new VideoGridWidget(ui->videoGridContainer);
    auto *drawerPanel = new QWidget(ui->videoGridContainer);
    drawerPanel->setObjectName("drawerPanel");
    drawerPanel->setFixedWidth(240);
    auto *deviceTree = new DeviceTreeWidget(drawerPanel);
    auto *drawerToggleBtn = new QPushButton(ui->videoGridContainer);
    drawerToggleBtn->setObjectName("drawerToggleBtn");
    drawerToggleBtn->setFixedSize(20, 40);

    // 窗口系统（标题栏 + 导航 + 原生事件）— HWND 创建前先注册，拦截 WM_NCCALCSIZE
    m_windowSystem = new WindowSystem(
        ui->titleBar,
        ui->btnMenu_Min, ui->btnMenu_Max, ui->btnMenu_Close,
        ui->btnNavMonitor, ui->btnNavPlayback, ui->btnNavLog, ui->btnNavSettings,
        ui->contentStack,
        ui->labelAppIcon, ui->labelAppTitle,
        this, this);

    // 命令日志（设为不阻止应用退出）
    auto *cmdLog = new CmdLogDialog(this);
    cmdLog->setAttribute(Qt::WA_QuitOnClose, false);
    cmdLog->show();

    // 设备管理器
    m_devMgr = new DeviceManager(m_cfg, deviceTree, videoGrid, cmdLog, this);

    // 地图控制器
    m_mapCtrl = new MapViewController(
        ui->widgetDisplay, videoGrid, drawerPanel, drawerToggleBtn,
        m_devMgr, m_trackMgr, m_cfg, this);

    // 设备交互控制器（所有业务逻辑）
    m_interactionCtrl = new DeviceInteractionController(
        ui, videoGrid, drawerPanel, deviceTree, drawerToggleBtn, this,
        m_devMgr, m_trackMgr, m_mapCtrl, m_cfg, m_devState,
        ui->statusbar, this);
}

MainWindow::~MainWindow()
{
    if (m_devMgr)
        m_devMgr->disconnectAll();
    delete ui;
    delete m_devState;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_mapCtrl)
        m_mapCtrl->updateLayout();
    if (m_interactionCtrl)
        m_interactionCtrl->handleResize();
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (m_windowSystem && m_windowSystem->handleNativeEvent(eventType, message, result))
        return true;
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (m_windowSystem)
        m_windowSystem->handleChangeEvent(event);
    QMainWindow::changeEvent(event);
}
