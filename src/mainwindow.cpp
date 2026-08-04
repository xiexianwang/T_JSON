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
#include "ptzforwarder.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_cfg(new ConfigManager(this))
    , m_trackMgr(new TrackManager(this))
    , m_mapCtrl(nullptr)
{
    ui->setupUi(this);

    m_devState = new DeviceState;

    auto *videoGrid = new VideoGridWidget(ui->widgetDisplay);
    auto *drawerPanel = new QWidget(ui->widgetDisplay);
    drawerPanel->setObjectName("drawerPanel");
    drawerPanel->setFixedWidth(240);
    auto *deviceTree = new DeviceTreeWidget(drawerPanel);
    auto *drawerToggleBtn = new QPushButton(ui->widgetDisplay);
    drawerToggleBtn->setObjectName("drawerToggleBtn");
    drawerToggleBtn->setFixedSize(20, 40);

    m_windowSystem = new WindowSystem(
        ui->titleBar,
        ui->btnMenu_Min, ui->btnMenu_Max, ui->btnMenu_Close,
        ui->btnNavMonitor, ui->btnNavPlayback, ui->btnNavLog, ui->btnNavSettings,
        ui->contentStack,
        ui->labelAppIcon, ui->labelAppTitle,
        this, this);

    auto *cmdLog = new CmdLogDialog(this);
    cmdLog->setAttribute(Qt::WA_QuitOnClose, false);
    
    m_ptzForwarder = new PtzForwarder(this);
    QTimer::singleShot(0, this, [this]() {
        if (m_cfg->serialServerEnabled()) {
            m_ptzForwarder->start(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
            m_ptzForwarder->setOffsets(m_cfg->ptzPanOffset(), m_cfg->ptzTiltOffset());
        }
    });

    m_devMgr = new DeviceManager(m_cfg, deviceTree, videoGrid, cmdLog, this);

    m_mapCtrl = new MapViewController(
        ui->widgetDisplay, videoGrid, drawerPanel, drawerToggleBtn,
        m_devMgr, m_trackMgr, m_cfg, this);

    m_interactionCtrl = new DeviceInteractionController(
        ui, videoGrid, drawerPanel, deviceTree, drawerToggleBtn, this,
        m_devMgr, m_trackMgr, m_mapCtrl, m_cfg, m_devState, m_ptzForwarder,
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
