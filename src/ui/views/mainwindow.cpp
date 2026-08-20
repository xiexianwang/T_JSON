//============================================================================
// mainwindow.cpp - T-JSON 主窗口实现
// 包含主窗口的构造/析构、UI 样式初始化、信号-槽连接，
// 以及完整的 JSON 帧解析、状态更新、地图坐标转换、云台镜头控制逻辑。
//============================================================================
#include "mainwindow.h"
#include "service/DeviceManager.h"
#include "core/GeoCalculator.h"
#include "ui_mainwindow.h"
#include <QAbstractButton>
#include <QLineEdit>
#include "ui/components/VideoGridWidget.h"
#include "ui/components/DeviceTreeWidget.h"

#include "infrastructure/rtspthread.h"
#include "ui/views/videowidget.h"
#include "ui/views/mapwidget.h"
#include "ui/views/cmdlogdialog.h"
#include "ui/views/MainWindowNavigation.h"
#include "ui/views/MainWindowDialogService.h"
#include "ui/views/MainWindowLayoutService.h"
#include "ui/views/MainWindowSystemService.h"
#include "ui/views/MainWindowControlService.h"
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>



//============================================================================
// 构造函数：初始化所有子模块、建立信号-槽连接、配置 UI
//============================================================================
// FORCE RECOMPILE
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_cfg(new ConfigManager(this))
    , m_presenter(new MainPresenter(this, m_cfg, this))           // RTSP 视频拉流线程
{
    ui->setupUi(this);

    setWindowIcon(QIcon(QStringLiteral(":/qss/logo.ico")));

    setupUiStyles();

    // Replace old single videoWidget with VideoGridWidget
    ui->videoWidget->hide();
    m_videoGrid = new VideoGridWidget(ui->widgetDisplay);
    if (ui->widgetDisplay->layout()) {
        ui->widgetDisplay->layout()->addWidget(m_videoGrid);
    }
    m_videoGrid->bindDevice(m_presenter->currentDeviceId());


    
    // --- 动态添加设备列表侧边栏 (Drawer) ---
    m_deviceTree = new DeviceTreeWidget(ui->centralwidget);
    m_deviceTree->setFixedWidth(260); // 固定的抽屉宽度
    
    // 插入到水平布局的最左侧（widgetDisplay 的左边）
    ui->horizontalLayout_middle->insertWidget(0, m_deviceTree);

    // 动态添加一个切换侧边栏的按钮到顶部导航栏
    QToolButton* btnToggleTree = new QToolButton(ui->titleBar);
    btnToggleTree->setText(QString::fromUtf8("设备列表"));
    btnToggleTree->setCheckable(true);
    btnToggleTree->setChecked(true);
    
    // 提取原有的按钮样式函数以便复用
    btnToggleTree->setIcon(QIcon(":/monitor.svg")); // 临时使用同样图标，或不用
    btnToggleTree->setIconSize(QSize(18, 18));
    btnToggleTree->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btnToggleTree->setStyleSheet("QToolButton { color: #cccccc; border: none; padding: 5px; } QToolButton:checked { color: #00aaff; }");

    // 将其插入到标题栏左侧（标题文字后面）
    ui->horizontalLayout_titleRow->insertWidget(1, btnToggleTree);

    connect(btnToggleTree, &QToolButton::toggled, m_deviceTree, &QWidget::setVisible);

    connect(m_deviceTree, &DeviceTreeWidget::channelDoubleClicked, m_presenter, &MainPresenter::onDeviceDoubleClicked);
    connect(m_deviceTree, &DeviceTreeWidget::deviceRemoved, m_presenter, &MainPresenter::onDeviceRemoved);
    connect(m_deviceTree, &DeviceTreeWidget::deviceToggleConnect, m_presenter, &MainPresenter::onDeviceToggleConnect);

    ui->titleBar->installEventFilter(this);
    ui->titleBar->setProperty("form", "title");
    ui->labelAppIcon->setPixmap(QPixmap(QStringLiteral(":/qss/logo.png")).scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->btnMenu_Min->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/minimize.png")));
    ui->btnMenu_Max->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/maximize.png")));
    ui->btnMenu_Close->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/close.png")));
    for (auto *b : {ui->btnMenu_Min, ui->btnMenu_Max, ui->btnMenu_Close})
        b->setIconSize(QSize(18, 18));

    // 为导航栏按钮设置 SVG 图标（图片在上，文字在下）
    auto setupNavBtn = [](QToolButton* btn, const QString& svgPath) {
        btn->setIcon(QIcon(svgPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    };
    setupNavBtn(ui->btnNavMonitor, QStringLiteral(":/monitor.svg"));
    setupNavBtn(ui->btnNavPlayback, QStringLiteral(":/playback.svg"));
    setupNavBtn(ui->btnNavLog, QStringLiteral(":/log.svg"));
    setupNavBtn(ui->btnNavSettings, QStringLiteral(":/gear.svg"));

    // 导航按钮互斥组
    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(ui->btnNavMonitor, 0);
    navGroup->addButton(ui->btnNavPlayback, 1);
    navGroup->addButton(ui->btnNavLog, 2);
    navGroup->addButton(ui->btnNavSettings, 3);
    ui->btnNavMonitor->setChecked(true);

    // 初始化导航服务
    m_navigation = new MainWindowNavigation(this);
    m_navigation->setup({ui->btnNavMonitor, ui->btnNavPlayback, ui->btnNavLog, ui->btnNavSettings},
                        ui->btnMapToggle, m_cfg, m_presenter, this);

    // 初始化对话框服务
    m_dialogService = new MainWindowDialogService(this);
    m_dialogService->setup(m_cfg, m_presenter, this);

    // 根据配置自动初始化电机通道
    m_presenter->initMotorChannel();

    // PTZ Forwarder start（延迟到事件循环启动后）
    QTimer::singleShot(0, this, [this]() {
        m_presenter->initPtzForwarder();
    });

    // 迷你地图：容器 → MapWidget → 覆盖层
    m_mapContainer = new QWidget(ui->widgetDisplay);
    m_mapContainer->setVisible(false);
    m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);
    m_mapWidget = new MapWidget(m_mapContainer);
    m_mapWidget->setGeometry(0, 0, 280, 280);
    // 透明覆盖层：迷你模式拦截鼠标（拖拽移动，双击展开）
    m_mapOverlay = new QWidget(m_mapContainer);
    m_mapOverlay->setGeometry(0, 0, 280, 280);
    m_mapOverlay->setCursor(Qt::OpenHandCursor);
    m_mapOverlay->installEventFilter(this);

    // PiP 独立对话框：大地图时视频显示于此
    // 保持 2566:1520 宽高比，置顶显示，高度需加上标题栏22px
    m_pipDialog = new QDialog(this, Qt::FramelessWindowHint | Qt::Tool);
    m_pipDialog->setAttribute(Qt::WA_ShowWithoutActivating);
    int pipW = 380;
    int pipH = static_cast<int>(pipW * 1520.0 / 2566.0) + 44;  // 226 + 44 = 270
    m_pipDialog->setFixedSize(pipW, pipH);
    auto *pipLay = new QVBoxLayout(m_pipDialog);
    pipLay->setSpacing(0);
    pipLay->setContentsMargins(0, 0, 0, 0);
    m_pipTitle = new QWidget(m_pipDialog);
    m_pipTitle->setFixedHeight(22);
    m_pipTitle->setCursor(Qt::OpenHandCursor);
    m_pipTitle->installEventFilter(this);
    m_pipTitle->setStyleSheet(QStringLiteral(
        "background-color:#2d2d2d; border-bottom:1px solid #1a1a1a;"
    ));
    auto *titleLay = new QHBoxLayout(m_pipTitle);
    titleLay->setContentsMargins(0, 0, 2, 0);
    titleLay->addStretch();
    auto *btnClose = new QPushButton(QStringLiteral("✕"), m_pipTitle);
    btnClose->setFixedSize(20, 20);
    btnClose->setCursor(Qt::ArrowCursor);
    btnClose->setStyleSheet(QStringLiteral(
        "QPushButton{background:transparent;color:#a0a0b0;border:none;font-size:13px;}"
        "QPushButton:hover{background:#e74c3c;color:#fff;border-radius:2px;}"
    ));
    connect(btnClose, &QPushButton::clicked, this, [this]() { m_pipDialog->hide(); });
    pipLay->addWidget(m_pipTitle);

    m_layoutService = new MainWindowLayoutService(this);
    m_layoutService->setup({
        ui->widgetDisplay, m_mapContainer, m_mapWidget, m_mapOverlay,
        m_pipDialog, m_pipTitle, m_videoGrid, ui->btnMapToggle,
        [this]() {
            if (m_videoGrid->parent() != ui->widgetDisplay) {
                m_videoGrid->setParent(ui->widgetDisplay);
                ui->verticalLayout_display->addWidget(m_videoGrid);
            }
            m_videoGrid->setVisible(true);
        },
        [](const QString& value) { return GeoCalculator::parseCoord(value); },
        [this]() { return qMakePair(ui->statLatitude->text(), ui->statLongitude->text()); }
    });
    connect(m_mapWidget, &MapWidget::miniRequested,
            m_layoutService, &MainWindowLayoutService::toggleMapMode);
    connect(m_mapWidget, &MapWidget::closeRequested,
            m_layoutService, &MainWindowLayoutService::toggleMap);
    connect(m_mapWidget, &MapWidget::enlargeRequested,
            m_layoutService, &MainWindowLayoutService::toggleMapMode);
    connect(ui->btnMapToggle, &QPushButton::clicked,
            m_layoutService, &MainWindowLayoutService::toggleMap);
    m_layoutService->updateMapLayout();

    // 系统参数轮询：500ms 周期查询设备 ImageSetting
    

    // AIInfo 超时清理：设备无目标时不发帧，2 秒无更新则清除残留标记
    

    // 恢复上次的开关状态
    ui->checkDigitalZoom->setChecked(m_cfg->digitalZoomEnabled());
    ui->checkAutoZoom->setChecked(m_cfg->autoZoomEnabled());
    ui->checkCaptureUpload->setChecked(m_cfg->captureUploadEnabled());
    ui->checkPosReset->setChecked(m_cfg->posResetEnabled());

    // 初始化框选启用状态
    int wm = ui->comboWorkMode->currentIndex();
    if (auto vw = m_videoGrid->getWidget(m_presenter->currentDeviceId()))
        vw->setSelectionEnabled(wm == 3 || wm == 4);

    //============================================================================
    // RTSP 视频流信号连接
    // RtspThread 在工作线程中拉流解码，通过信号将帧数据传回主线程
    // VideoGridWidget 统一透传每个设备的 selectionFinished 信号用于框选跟踪
    //============================================================================
    connect(m_videoGrid, &VideoGridWidget::selectionFinished, this, &MainWindow::onVideoSelection);

    //============================================================================
    // T-JSON 协议信号连接
    // TJsonClient 管理 TCP 长连接、心跳保活、JSON 帧收发与自动重连
    //============================================================================
    
    // 自动重连信号：每次重连尝试时更新按钮文本与状态栏提示

    // 重连失败：恢复按钮初始状态


    //============================================================================
    // 控制服务：PTZ/镜头/预置位/雨刷电机/附加功能开关
    //============================================================================
    m_controlService = new MainWindowControlService(this);
    m_controlService->setup({
        ui->btnPtzUp, ui->btnPtzDown, ui->btnPtzLeft, ui->btnPtzRight,
        ui->btnPtzTopLeft, ui->btnPtzTopRight, ui->btnPtzBottomLeft, ui->btnPtzBottomRight,
        ui->sliderSpeed, ui->spinSpeed,
        ui->btnZoomIn, ui->btnZoomOut, ui->btnFocusIn, ui->btnFocusOut,
        ui->sliderZoomSpeed, ui->spinZoomSpeed,
        ui->btnCallPreset, ui->btnSetPreset, ui->btnDelPreset, ui->btnPtzReset,
        ui->checkDigitalZoom, ui->checkAutoZoom, ui->checkCaptureUpload, ui->checkPosReset,
        ui->btnWiperStart, ui->btnWiperStop, ui->btnWiperLeft, ui->btnWiperRight,
        ui->btnWiperZeroCalib, ui->btnWiperMode, ui->btnWiperSilent,
        ui->editWiperCurrent, ui->statWiperStatus, ui->statusbar
    }, m_cfg, m_presenter,
        [this]() { return requireConnected(); },
        [this]() { return requireMotorReady(); });

    //============================================================================
    // 指令日志窗口
    // 实时显示所有下发给设备的指令内容，方便调试与协议分析
    //============================================================================
    m_logDialog = new CmdLogDialog(this);
    connect(m_presenter, &MainPresenter::commandSentToLog, m_logDialog, &CmdLogDialog::appendLog);
    m_navigation->setLogDialog(m_logDialog);
    m_dialogService->setLogDialog(m_logDialog);

    auto* trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(QStringLiteral(":/qss/logo.ico")));
    trayIcon->setToolTip(QStringLiteral("LSS视频管理客户端"));
    auto* trayMenu = new QMenu(this);
    m_systemService = new MainWindowSystemService(this);
    m_systemService->setup({
        ui->btnMenu_Min, ui->btnMenu_Max, ui->btnMenu_Close, trayIcon, trayMenu, m_cfg,
        [this, trayIcon]() {
            if (m_dialogService->showCloseConfirmation()) {
                trayIcon->hide();
                qApp->quit();
            }
        },
        [this]() {
            m_presenter->closeVideoStream();
            if (auto vw = m_videoGrid->getWidget(m_presenter->currentDeviceId())) vw->clearFrame();
            if (m_presenter->isDeviceConnected()) m_presenter->disconnectDevice();
        },
        [this]() { return isMaximized(); },
        [this]() { showMinimized(); },
        [this]() { isMaximized() ? showNormal() : showMaximized(); },
        [this]() { showNormal(); activateWindow(); raise(); },
        [this]() { hide(); },
        [this]() { show(); }
    });

    for (auto *cb : findChildren<QComboBox *>()) {
        cb->setFocusPolicy(Qt::StrongFocus);
        cb->installEventFilter(this);
    }
    m_controlService->updateMotorButtons();
}

//============================================================================
// 析构函数：释放 UI 资源
// 子模块对象 (m_client, m_cfg, m_device, m_rtsp, m_mapWidget)
// 均以 MainWindow 为父对象，由 Qt 对象树自动析构
// 析构前停止 RTSP 线程：设置停止标志后 FFmpeg 中断回调会使其快速返回
//============================================================================
MainWindow::~MainWindow()
{
    // 1. 先断开 EventBus 对 MainPresenter 的信号连接，防止析构过程中信号回调访问已析构对象
    disconnect(m_presenter, nullptr, this, nullptr);

    // 2. 关闭所有设备（停止 RTSP 线程、断开 TCP、取消自动重连）
    DeviceManager::instance()->removeAllDevices();

    delete m_pipDialog;
    delete ui;
}

//============================================================================
// 标题栏按钮
//============================================================================

void MainWindow::on_btnMenu_Min_clicked()
{
    m_systemService->onMinimize();
}

void MainWindow::on_btnMenu_Max_clicked()
{
    m_systemService->onMaximize();
}

void MainWindow::on_btnMenu_Close_clicked()
{
    m_systemService->onClose();
}

//============================================================================
// 系统托盘
//============================================================================

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_systemService->handleCloseEvent(event);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{ m_systemService->onTrayActivated(reason); }

void MainWindow::onTrayShow()
{
    m_systemService->onTrayShow();
}

void MainWindow::onTrayExit()
{
    m_systemService->onTrayExit();
}

//============================================================================
// 导航按钮
//============================================================================

void MainWindow::on_btnNavMonitor_clicked()  { /* 当前页面 */ }
void MainWindow::on_btnNavPlayback_clicked() { /* 预留 */ }
void MainWindow::on_btnNavLog_clicked()      {
    m_navigation->onBtnNavLogClicked();
}
void MainWindow::on_btnNavSettings_clicked() {
    m_navigation->onBtnNavSettingsClicked();
    m_controlService->updateMotorButtons();
}

//============================================================================
// changeEvent - 窗口状态变化时更新最大化按钮图标
//============================================================================

void MainWindow::changeEvent(QEvent *event)
{
    m_systemService->handleChangeEvent(event);
    QMainWindow::changeEvent(event);
}

//============================================================================
// nativeEvent - 拦截 Windows 消息实现自定义标题栏
//============================================================================

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        switch (msg->message) {

        case WM_NCCALCSIZE: {
            RECT *rc;
            if (msg->wParam) {
                NCCALCSIZE_PARAMS *p = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
                rc = &p->rgrc[0];
            } else {
                rc = reinterpret_cast<RECT*>(msg->lParam);
            }
            if (IsZoomed(msg->hwnd)) {
                // 最大化时 Windows 会在四边添加不可见边框导致内容偏移
                // 补偿边框宽度使客户区填满工作区
                int border = GetSystemMetrics(SM_CXSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
                rc->left   += border;
                rc->top    += border;
                rc->right  -= border;
                rc->bottom -= border;
            }
            *result = 0;
            return true;
        }

        case WM_NCHITTEST: {
            // GET_X_LPARAM 返回物理像素坐标，需 / devicePixelRatioF() 转为 Qt 逻辑坐标
            POINT nativePt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
            ScreenToClient(msg->hwnd, &nativePt);
            qreal dpr = devicePixelRatioF();
            QPoint local(qRound(nativePt.x / dpr), qRound(nativePt.y / dpr));

            if (!isMaximized()) {
                int b = 8;
                if (local.y() <= b && local.x() <= b) { *result = HTTOPLEFT; return true; }
                if (local.y() <= b && local.x() >= width() - b) { *result = HTTOPRIGHT; return true; }
                if (local.y() >= height() - b && local.x() <= b) { *result = HTBOTTOMLEFT; return true; }
                if (local.y() >= height() - b && local.x() >= width() - b) { *result = HTBOTTOMRIGHT; return true; }
                if (local.y() <= b) { *result = HTTOP; return true; }
                if (local.y() >= height() - b) { *result = HTBOTTOM; return true; }
                if (local.x() <= b) { *result = HTLEFT; return true; }
                if (local.x() >= width() - b) { *result = HTRIGHT; return true; }
            }

            if (ui->titleBar && ui->titleBar->isVisible()) {
                QPoint tl = ui->titleBar->mapFromParent(local);
                if (ui->titleBar->rect().contains(tl)) {
                    QWidget *child = ui->titleBar->childAt(tl);
                    QWidget *p = child;
                    bool isInteractive = false;
                    while (p && p != ui->titleBar) {
                        if (qobject_cast<QAbstractButton*>(p) || qobject_cast<QLineEdit*>(p)) {
                            isInteractive = true;
                            break;
                        }
                        p = p->parentWidget();
                    }
                    if (isInteractive) {
                        *result = HTCLIENT;
                        return true;
                    }
                    *result = HTCAPTION;
                    return true;
                }
            }
            *result = HTCLIENT;
            return true;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            RECT wa;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
            int border = GetSystemMetrics(SM_CXSIZEFRAME)
                       + GetSystemMetrics(SM_CXPADDEDBORDER);
            mmi->ptMaxPosition.x = wa.left - border;
            mmi->ptMaxPosition.y = wa.top - border;
            mmi->ptMaxSize.x = (wa.right - wa.left) + border * 2;
            mmi->ptMaxSize.y = (wa.bottom - wa.top) + border * 2;
            mmi->ptMinTrackSize.x = minimumWidth();
            mmi->ptMinTrackSize.y = minimumHeight();
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
    return QMainWindow::nativeEvent(eventType, message, result);
}

//============================================================================
// setupUiStyles - 加载并应用 QSS 样式表
//============================================================================
void MainWindow::setupUiStyles()
{
    ui->tableIdentify->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    QFile file(QStringLiteral(":/style.qss"));
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QByteArray qss = file.readAll();
        qDebug() << "[QSS] Loaded" << qss.size() << "bytes from :/style.qss";
        qApp->setStyleSheet(QString::fromUtf8(qss));
        file.close();
    } else {
        qWarning() << "[QSS] FAILED to open :/style.qss";
    }

    // QScrollArea viewport 默认继承系统托盘色，强制设为暗黑
    if (ui->scrollAreaControl) {
        ui->scrollAreaControl->viewport()->setAutoFillBackground(true);
        QPalette pal = ui->scrollAreaControl->viewport()->palette();
        pal.setColor(QPalette::Window, QColor(0x44, 0x44, 0x44));
        ui->scrollAreaControl->viewport()->setPalette(pal);
    }
}

//============================================================================
// on_btnConnect_clicked - 连接/断开设备按钮
// 已连接时点击为断开；未连接时读取 IP 和端口发起 TCP 连接
//============================================================================
void MainWindow::on_btnConnect_clicked()
{
    m_presenter->on_btnConnect_clicked();
}



//============================================================================
// on_btnCancelConnect_clicked - 取消正在进行的连接
// 直接断开 TCP 连接并恢复按钮状态
//============================================================================
void MainWindow::on_btnCancelConnect_clicked()
{
    m_presenter->on_btnCancelConnect_clicked();
}



//============================================================================
// on_btnVideoConnect_clicked - 连接 RTSP 视频流
// 从输入框获取 RTSP URL 后交给 RtspThread 进行拉流
//============================================================================
void MainWindow::on_btnVideoConnect_clicked()
{
    m_presenter->on_btnVideoConnect_clicked();
}



//============================================================================
// on_btnVideoDisconnect_clicked - 断开 RTSP 视频流
// 停止拉流线程、清除视频画面、恢复按钮状态
//============================================================================
void MainWindow::on_btnVideoDisconnect_clicked()
{
    m_presenter->on_btnVideoDisconnect_clicked();
}



//============================================================================
// onRtspFrame - 收到一帧 RTSP 视频图像
// 将解码后的 QImage 传递给 VideoWidget 进行渲染
//============================================================================


//============================================================================
// onRtspOpened - RTSP 视频流成功打开
// 更新按钮文本与状态栏提示
//============================================================================
void MainWindow::onRtspOpened()
{
    ui->btnVideoConnect->setEnabled(false);
    ui->btnVideoConnect->setText(QString::fromUtf8("已连接"));
    ui->statusbar->showMessage(QString::fromUtf8("RTSP 视频已连接"), 3000);
}

//============================================================================
// onRtspError - RTSP 视频流错误处理
// 清除画面、恢复按钮，并在状态栏显示错误信息
//============================================================================
void MainWindow::onRtspError(const QString &msg)
{
    if (auto vw = m_videoGrid->getWidget(m_presenter->currentDeviceId())) vw->clearFrame();
    if (m_presenter->isVideoStreamRunning()) {
        // 线程还在运行说明是自动重连中，保持按钮在"重连中..."状态
        ui->btnVideoConnect->setText(QString::fromUtf8("重连中..."));
        ui->statusbar->showMessage(msg.isEmpty()
            ? QString::fromUtf8("RTSP 断开，正在重连...")
            : QString::fromUtf8("RTSP 重连失败，继续重试..."));
    } else {
        // 线程已退出，按钮恢复"开启"让用户手动再试
        ui->btnVideoConnect->setEnabled(true);
        ui->btnVideoConnect->setText(QString::fromUtf8("开启"));
        ui->statusbar->showMessage(msg);
    }
}

//============================================================================
// onVideoSelection - 用户在视频画面上的框选操作
// 将框选的像素坐标与宽高发送给设备，用于框选跟踪模式
// cx, cy 为框选区域中心像素坐标，pw, ph 为框宽高
//============================================================================
void MainWindow::onVideoSelection(const QString& deviceId, int cx, int cy, int pw, int ph)
{
    m_presenter->onVideoSelection(deviceId, cx, cy, pw, ph);
}

//============================================================================
// onDeviceConnected - 设备连接成功回调
// 更新按钮样式为红色"断开连接"，自动查询设备当前图像参数
//============================================================================
void MainWindow::onDeviceConnected()
{
    ui->btnConnect->setText(QString::fromUtf8("断开连接"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", "connected");
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("已连接到设备"), 3000);

    // 连接成功后自动请求一次图像参数，以便 UI 与设备状态同步

    // 首次连接设备时自动打开 RTSP，后续不再覆盖用户操作
    if (!m_rtspEverOpened) {
        QString rtspUrl = ui->lineEditRtsp->text().trimmed();
        if (!rtspUrl.isEmpty()) {
            m_rtspEverOpened = true;
            ui->btnVideoConnect->setEnabled(false);
            ui->btnVideoConnect->setText(QString::fromUtf8("连接中..."));
            m_presenter->startVideoStream(rtspUrl);
        }
    }
}

//============================================================================
// onDeviceDisconnected - 设备断开回调
// 恢复连接按钮的初始外观
//============================================================================
void MainWindow::onDeviceDisconnected()
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", QVariant());
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("设备已断开"), 3000);

    // 停止系统参数定时下发
}



//============================================================================
// onErrorOccurred - 连接错误处理
// 非重连时弹框显示错误；重连中只在状态栏提示，继续自动重连
//============================================================================
void MainWindow::onErrorOccurred(const QString& errorMsg)
{
    if (ui->btnConnect->property("state").toString() == QStringLiteral("reconnecting")) {
        ui->statusbar->showMessage(QString::fromUtf8("重连失败，%1").arg(errorMsg), 3000);
        return;
    }

    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", QVariant());
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    QMessageBox::warning(this, QString::fromUtf8("连接错误"), errorMsg);
}

//============================================================================
// onAckReceived - 已迁移至 MainPresenter::showAck（经 EventBus 回调）
//============================================================================

//============================================================================


//============================================================================
// onImageSnapped - 设备抓拍图像回调
// 将 JPEG 数据保存到 snapshots 目录，文件名为 yyyyMMdd_HHmmss_zzz.jpg
// 状态栏显示保存路径及图像在画面中的位置信息
//============================================================================
void MainWindow::onImageSnapped(const QByteArray& jpegData, const QRect& location)
{
    QString dirPath = qApp->applicationDirPath() + QStringLiteral("/snapshots");
    QDir().mkpath(dirPath);

    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString path = dirPath + QStringLiteral("/snap_") + ts + QStringLiteral(".jpg");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(jpegData);
        f.close();
        ui->statusbar->showMessage(
            QString::fromUtf8("已保存抓拍: %1  位置: (%2,%3 %4x%5)")
                .arg(path)
                .arg(location.x()).arg(location.y())
                .arg(location.width()).arg(location.height()),
            5000);
    }
}

//============================================================================

//============================================================================
// requireConnected - 未连接时弹出提示并返回 false
// 所有需要连接设备才能执行的 UI 操作均应先调用此函数
//============================================================================
bool MainWindow::requireConnected()
{
    return m_dialogService->requireConnected();
}

// 检查电机是否就绪
// MODBUS-RTU 协议时需串口已打开，Pelco-D 直发即可
bool MainWindow::requireMotorReady()
{
    return m_dialogService->requireMotorReady();
}

//============================================================================
// on_comboWorkMode_currentIndexChanged - 工作模式下拉框切换
//   0 = 关闭AI, 1 = 目标识别, 2 = 自动跟踪, 3 = 点选跟踪, 4 = 框选跟踪
//============================================================================
void MainWindow::on_comboWorkMode_currentIndexChanged(int index)
{
    m_presenter->onComboWorkModeChanged(index);
}

//============================================================================
// on_btnPtzMoveTo_clicked - 云台转到指定角度
//============================================================================
void MainWindow::on_btnPtzMoveTo_clicked()
{
    m_presenter->on_btnPtzMoveTo_clicked();
}



//============================================================================
// on_btnPtzMoveToGps_clicked - 云台转动到指定经纬度高度
//============================================================================
void MainWindow::on_btnPtzMoveToGps_clicked()
{
    m_presenter->on_btnPtzMoveToGps_clicked();
}



//============================================================================
// on_btnPanZeroCalib_clicked - 水平零点标定
//============================================================================
void MainWindow::on_btnPanZeroCalib_clicked()
{
    m_presenter->on_btnPanZeroCalib_clicked();
}



//============================================================================
//============================================================================
// on_comboAlgoModel1/2_currentIndexChanged - 算法模型下拉框切换
// 受 m_updatingFromDevice 保护，避免设备回传时重复下发指令
//============================================================================
void MainWindow::on_comboAlgoModel1_currentIndexChanged(int index)
{
    int low = ui->comboAlgoModel2->currentIndex();
    m_presenter->sendAlgoModel(index * 10 + (low >= 0 ? low + 2 : 0));
}
void MainWindow::on_comboAlgoModel2_currentIndexChanged(int index)
{
    int high = ui->comboAlgoModel1->currentIndex();
    m_presenter->sendAlgoModel(high * 10 + (index + 2));
}

//============================================================================
// on_comboDisplayMode_currentIndexChanged - 显示模式下拉框切换
// 切换时下发显示模式变更指令，镜头目标由显示模式自动判断
//============================================================================
void MainWindow::on_comboDisplayMode_currentIndexChanged(int index)
{
    m_presenter->onComboDisplayModeChanged(index);
}

//============================================================================
// on_comboLensTarget_currentIndexChanged - 已删除，镜头目标由显示模式自动判断
//============================================================================

//============================================================================
// on_btnSetLocation_clicked - 手动设置设备经纬度
// 从输入框读取经纬度字符串，直接下发给设备覆写 GPS 信息
//============================================================================
void MainWindow::on_btnSetLocation_clicked()
{
    m_presenter->on_btnSetLocation_clicked();
}



//============================================================================
// on_btnGetImageParams_clicked - 查询设备当前图像参数
// 设备会以 ImageSetting 类型帧回复，触发 updateStatusFromJson 更新 UI
//============================================================================
void MainWindow::on_btnGetImageParams_clicked()
{
    m_presenter->on_btnGetImageParams_clicked();
}





//============================================================================

//============================================================================

// Haversine 公式计算两点间距离（米）


// ── 轨迹点抽稀阈值 ──


// 视觉法距离估算：已知目标参考尺寸，用像素大小反推距离

//============================================================================
// resizeEvent - 窗口缩放时重新布局
//============================================================================
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_layoutService->updateMapLayout();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // QComboBox 滚轮拦截：未展开时忽略滚轮事件
    if (event->type() == QEvent::Wheel) {
        auto *cb = qobject_cast<QComboBox*>(obj);
        if (cb && !cb->view()->isVisible()) {
            return true;  // 吞掉滚轮事件
        }
    }

    if (m_layoutService && m_layoutService->handleEventFilter(obj, event)) return true;
    return QMainWindow::eventFilter(obj, event);
}


void MainWindow::refreshStyle(QWidget *w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}


//============================================================================
// ---- IMainView 接口实现 ----
//============================================================================
QWidget* MainWindow::asWidget() { return this; }

void MainWindow::showStatusMessage(const QString& msg, int timeoutMs)
{
    ui->statusbar->showMessage(msg, timeoutMs);
}

void MainWindow::setConnectButton(const QString& text, bool enabled,
                                  const QString& state, bool cancelVisible)
{
    ui->btnConnect->setText(text);
    ui->btnConnect->setEnabled(enabled);
    ui->btnConnect->setProperty("state", state.isEmpty() ? QVariant() : QVariant(state));
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(cancelVisible);
}

void MainWindow::setVideoConnectButton(const QString& text, bool enabled)
{
    ui->btnVideoConnect->setEnabled(enabled);
    ui->btnVideoConnect->setText(text);
}

QString MainWindow::ipText() const { return ui->lineEditIp->text(); }
QString MainWindow::rtspUrlText() const { return ui->lineEditRtsp->text(); }
QString MainWindow::targetPanText() const { return ui->editTargetPan->text(); }
QString MainWindow::targetTiltText() const { return ui->editTargetTilt->text(); }
QString MainWindow::targetLonText() const { return ui->editTargetLon->text(); }
QString MainWindow::targetLatText() const { return ui->editTargetLat->text(); }
QString MainWindow::targetAltText() const { return ui->editTargetAlt->text(); }
QString MainWindow::setLatText() const { return ui->editSetLat->text(); }
QString MainWindow::setLonText() const { return ui->editSetLon->text(); }
QString MainWindow::setHeightText() const { return ui->editSetHeight->text(); }
int MainWindow::wiperCurrentMa() const { return ui->editWiperCurrent->text().toInt(); }
int MainWindow::presetValue() const { return ui->spinPreset->value(); }
int MainWindow::workModeIndex() const { return ui->comboWorkMode->currentIndex(); }
int MainWindow::algoModel2Index() const { return ui->comboAlgoModel2->currentIndex(); }
int MainWindow::displayModeIndex() const { return ui->comboDisplayMode->currentIndex(); }

QString MainWindow::statLatitudeText() const { return ui->statLatitude->text(); }
QString MainWindow::statLongitudeText() const { return ui->statLongitude->text(); }
QString MainWindow::statPanAngleText() const { return ui->statPanAngle->text(); }
QString MainWindow::statTiltAngleText() const { return ui->statTiltAngle->text(); }

void MainWindow::showDeviceState(int camMode, const QString& lat, const QString& lon,
                                 const QString& height, const QString& pan, const QString& tilt)
{
    // 语义：null 字符串 = 不更新该字段；非 null 空串 = 清除；非空 = 设置
    if (camMode >= 0)
        ui->statCamMode->setText(QString::number(camMode));
    if (!lat.isNull())
        ui->statLatitude->setText(lat);
    if (!lon.isNull())
        ui->statLongitude->setText(lon);
    if (!height.isNull()) {
        if (height.isEmpty()) ui->statHeight->clear();
        else ui->statHeight->setText(height);
    }
    if (!pan.isNull()) {
        if (pan.isEmpty()) ui->statPanAngle->clear();
        else ui->statPanAngle->setText(pan);
    }
    if (!tilt.isNull()) {
        if (tilt.isEmpty()) ui->statTiltAngle->clear();
        else ui->statTiltAngle->setText(tilt);
    }
}

void MainWindow::showLensStats(double visZoom, double visFocal, double visHfov,
                               double irZoom, double irFocal, double irHfov)
{
    ui->statZoomVis->setText(QString::number(visZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalVis->setText(QString::number(visFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusVis->clear();
    ui->statFovVis->setText(QString::number(visHfov, 'f', 2) + QStringLiteral("°"));

    ui->statZoomIR->setText(QString::number(irZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusIR->clear();
    ui->statFovIR->setText(QString::number(irHfov, 'f', 2) + QStringLiteral("°"));
}

void MainWindow::setIdentifyCount(const QString& text)
{
    ui->lblIdentifyCount->setText(text);
}

void MainWindow::clearIdentifyTable()
{
    ui->tableIdentify->setRowCount(0);
}

void MainWindow::addIdentifyRow(const QString& id, int cls, double dist,
                                const QString& pos, const QString& miss)
{
    int r = ui->tableIdentify->rowCount();
    ui->tableIdentify->insertRow(r);
    ui->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
    ui->tableIdentify->setItem(r, 1, new QTableWidgetItem(QString::number(cls)));
    ui->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(dist, 'f', 1)));
    if (!pos.isNull())
        ui->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));
    if (!miss.isNull())
        ui->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
}

void MainWindow::showTrackStatus(const QString& text, const QString& state)
{
    ui->lblTrackStatus->setText(text);
    ui->lblTrackStatus->setProperty("state", state);
    refreshStyle(ui->lblTrackStatus);
}

void MainWindow::setTrackDistance(const QString& text)
{
    if (text.isEmpty()) ui->trackDistance->clear();
    else ui->trackDistance->setText(text);
}

void MainWindow::setTrackPos(const QString& text)
{
    if (text.isEmpty()) ui->trackPos->clear();
    else ui->trackPos->setText(text);
}

void MainWindow::setTrackMissDistance(const QString& text)
{
    if (text.isEmpty()) ui->trackMissDistance->clear();
    else ui->trackMissDistance->setText(text);
}

void MainWindow::showImageParams(const QString& resolution, const QString& bitrate,
                                 const QString& codec, const QString& workMode,
                                 const QString& pipShow, const QString& algoModel,
                                 const QString& maxVisFL, const QString& maxIRFL)
{
    ui->paramResolution->setText(resolution);
    ui->paramBitrate->setText(bitrate);
    ui->paramCodec->setText(codec);
    ui->paramWorkMode->setText(workMode);
    ui->paramPipShow->setText(pipShow);
    ui->paramAlgoModel->setText(algoModel);
    ui->paramMaxVisFL->setText(maxVisFL);
    ui->paramMaxIRFL->setText(maxIRFL);
}

void MainWindow::setAlgoModel1Index(int high)
{
    ui->comboAlgoModel1->blockSignals(true);
    ui->comboAlgoModel1->setCurrentIndex(high);
    ui->comboAlgoModel1->blockSignals(false);
}

void MainWindow::setAlgoModel2Index(int low)
{
    ui->comboAlgoModel2->blockSignals(true);
    ui->comboAlgoModel2->setCurrentIndex(low);
    ui->comboAlgoModel2->blockSignals(false);
}

void MainWindow::setDisplayModeIndex(int index)
{
    ui->comboDisplayMode->blockSignals(true);
    ui->comboDisplayMode->setCurrentIndex(index);
    ui->comboDisplayMode->blockSignals(false);
}

void MainWindow::setWorkModeIndex(int index)
{
    ui->comboWorkMode->blockSignals(true);
    ui->comboWorkMode->setCurrentIndex(index);
    ui->comboWorkMode->blockSignals(false);
}

void MainWindow::setDigitalZoomChecked(bool checked)
{
    ui->checkDigitalZoom->blockSignals(true);
    ui->checkDigitalZoom->setChecked(checked);
    ui->checkDigitalZoom->blockSignals(false);
}

void MainWindow::setAutoZoomChecked(bool checked)
{
    ui->checkAutoZoom->blockSignals(true);
    ui->checkAutoZoom->setChecked(checked);
    ui->checkAutoZoom->blockSignals(false);
}

void MainWindow::setCaptureUploadChecked(bool checked)
{
    ui->checkCaptureUpload->blockSignals(true);
    ui->checkCaptureUpload->setChecked(checked);
    ui->checkCaptureUpload->blockSignals(false);
}

void MainWindow::setPosResetChecked(bool checked)
{
    ui->checkPosReset->blockSignals(true);
    ui->checkPosReset->setChecked(checked);
    ui->checkPosReset->blockSignals(false);
}

void MainWindow::setVideoFrame(const QString& deviceId, const QImage& frame)
{
    if (!m_videoGrid) return;
    if (VideoWidget* vw = m_videoGrid->bindDevice(deviceId)) {
        vw->setFrame(frame);
    }
}

void MainWindow::clearVideoFrame(const QString& deviceId)
{
    if (!m_videoGrid) return;
    if (VideoWidget* vw = m_videoGrid->bindDevice(deviceId)) {
        vw->clearFrame();
    }
}

void MainWindow::setVideoSelectionEnabled(const QString& deviceId, bool enabled)
{
    if (!m_videoGrid) return;
    if (VideoWidget* vw = m_videoGrid->bindDevice(deviceId)) {
        vw->setSelectionEnabled(enabled);
    }
}

void MainWindow::repaintVideoGrid()
{
    if (!m_videoGrid) return;
    m_videoGrid->repaint();
}

void MainWindow::mapClearAllTracks()
{
    if (!m_mapWidget) return;
    m_mapWidget->clearAllTracks();
}

void MainWindow::mapUpdateTargetMarkers(const QJsonArray& targets)
{
    if (!m_mapWidget) return;
    m_mapWidget->updateTargetMarkers(targets);
}

void MainWindow::mapClearFov()
{
    if (!m_mapWidget) return;
    m_mapWidget->clearFov();
}

void MainWindow::mapAppendTrackPoint(const QString& trackId, double lat, double lon, double speed)
{
    if (!m_mapWidget) return;
    m_mapWidget->appendTrackPoint(trackId, lat, lon, speed);
}

void MainWindow::mapSetDevicePosition(double lat, double lon)
{
    if (!m_mapWidget) return;
    m_mapWidget->setDevicePosition(lat, lon);
}

void MainWindow::mapSetVisFov(double lat, double lon, double panDeg, double tiltDeg,
                              double hfov, double vfov, double distance)
{
    if (!m_mapWidget) return;
    m_mapWidget->setVisFov(lat, lon, panDeg, tiltDeg, hfov, vfov, distance);
}

void MainWindow::mapSetIrFov(double lat, double lon, double panDeg, double tiltDeg,
                             double hfov, double vfov, double distance)
{
    if (!m_mapWidget) return;
    m_mapWidget->setIrFov(lat, lon, panDeg, tiltDeg, hfov, vfov, distance);
}

void MainWindow::mapSetDeviceInfo(double lat, double lon, double alt, double pan, double tilt,
                                  double visHfov, double visVfov, double range, bool rangeEstimated)
{
    if (!m_mapWidget) return;
    m_mapWidget->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, rangeEstimated);
}

void MainWindow::onDeviceReconnecting(int attempt, int maxRetries) {
    QString total = maxRetries > 0 ? QString("/%1").arg(maxRetries) : QStringLiteral("");
    ui->btnConnect->setText(QString::fromUtf8("连接中(重试:%1%2)").arg(attempt).arg(total));
    ui->btnConnect->setEnabled(false);
    ui->btnConnect->setProperty("state", "reconnecting");
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(true);
    ui->statusbar->showMessage(QString::fromUtf8("断开，正在重连 %1 次...").arg(attempt));
}

void MainWindow::onDeviceReconnectFailed() {
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", QVariant());
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("重连 10 次失败，请检查设备连接"), 5000);
}
