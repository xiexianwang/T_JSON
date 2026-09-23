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
#include <QHeaderView>
#include "ui/components/VideoGridWidget.h"
#include "ui/components/DeviceTreeWidget.h"

#include "ui/views/mapwidget.h"
#include "ui/views/cmdlogdialog.h"
#include "ui/views/MainWindowNavigation.h"
#include "ui/views/MainWindowDialogService.h"
#include "ui/views/MainWindowLayoutService.h"
#include "ui/views/MainWindowSystemService.h"
#include "ui/views/MainWindowControlService.h"
#include "ui/views/devicepropertiesdialog.h"
#include "ui/views/wheelredirectfilter.h"
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QTimer>
#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QLocale>
#include <QRegularExpression>
#include <QRegularExpressionValidator>



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
    setupInputValidators();

    // Replace old single videoWidget with VideoGridWidget
    ui->videoWidget->hide();
    m_videoGrid = new VideoGridWidget(ui->widgetDisplay);
    if (ui->widgetDisplay->layout()) {
        ui->widgetDisplay->layout()->addWidget(m_videoGrid);
    }
    // 不预绑定 default_device，VideoWidget 在真正连接设备时按需创建


    
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

    connect(m_deviceTree, &DeviceTreeWidget::deviceActivated, m_presenter, &MainPresenter::onDeviceActivated);
    connect(m_deviceTree, &DeviceTreeWidget::deviceRemoved, m_presenter, &MainPresenter::onDeviceRemoved);
    connect(m_deviceTree, &DeviceTreeWidget::deviceToggleConnect, m_presenter, &MainPresenter::onDeviceToggleConnect);
    connect(m_deviceTree, &DeviceTreeWidget::layoutModeChanged, m_videoGrid, &VideoGridWidget::setLayoutMode);
    // click video tile -> switch current device (focus only, keep streams)
    connect(m_videoGrid, &VideoGridWidget::deviceClicked, this, [this](const QString& deviceId) {
        const DeviceEntry entry = m_deviceTree->entryForId(deviceId);
        DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
        if (ctx && ctx->isConnected()) {
            m_presenter->selectDevice(deviceId);              // connected: focus only
        } else if (entry.id == deviceId) {
            m_presenter->onDeviceActivated(deviceId, entry);  // offline: full connect
        }
    });
    // double-click video tile -> toggle enlarge (plan A), and focus that device
    connect(m_videoGrid, &VideoGridWidget::deviceDoubleClicked, this, [this](const QString& deviceId) {
        m_videoGrid->toggleFocus(deviceId);
        const DeviceEntry entry = m_deviceTree->entryForId(deviceId);
        DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
        if (ctx && ctx->isConnected()) {
            m_presenter->selectDevice(deviceId);
        } else if (entry.id == deviceId) {
            m_presenter->onDeviceActivated(deviceId, entry);
        }
    });
    // tree structure changed -> recompute numbers and refresh video badges
    connect(m_deviceTree, &DeviceTreeWidget::treeModified, this, [this]() {
        refreshDeviceLabelsAndActive();
    });
    connect(m_deviceTree, &DeviceTreeWidget::devicePropertiesRequested, this, [this](const QString& deviceId) {
        DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
        DeviceConfig cfg = m_deviceTree->entryForId(deviceId).config;
        if (ctx) cfg = ctx->deviceConfig();

        DevicePropertiesDialog dlg(m_deviceTree->entryForId(deviceId).ip, &cfg, this);
        if (dlg.exec() != QDialog::Accepted) return;

        m_deviceTree->setConfigForId(deviceId, cfg);
        if (ctx) {
            ctx->setDeviceConfig(cfg);
            // 已连接时重新初始化电机/转台通道
            if (ctx->isConnected()) {
                m_presenter->applyMotorChannelForDevice(deviceId);
                m_presenter->initPtzForwarderForDevice(deviceId);
            }
        }
        // 协议可能已改变：按新协议刷新电机按钮/电流项（保持电流/延迟仅 STM32 可用）
        m_controlService->updateMotorButtons();
    });

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
                        m_cfg, m_presenter, this);

    // 初始化对话框服务
    m_dialogService = new MainWindowDialogService(this);
    m_dialogService->setup(m_cfg, m_presenter, this);

    // 电机/转台初始化现在跟随设备连接（在 toggleDeviceConnect 中调用）
    // 不再在启动时全局初始化

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
    MainWindowControlService::ControlWidgets cw;
    cw.ptzUp = ui->btnPtzUp;
    cw.ptzDown = ui->btnPtzDown;
    cw.ptzLeft = ui->btnPtzLeft;
    cw.ptzRight = ui->btnPtzRight;
    cw.ptzTopLeft = ui->btnPtzTopLeft;
    cw.ptzTopRight = ui->btnPtzTopRight;
    cw.ptzBottomLeft = ui->btnPtzBottomLeft;
    cw.ptzBottomRight = ui->btnPtzBottomRight;
    cw.sliderSpeed = ui->sliderSpeed;
    cw.spinSpeed = ui->spinSpeed;
    cw.zoomIn = ui->btnZoomIn;
    cw.zoomOut = ui->btnZoomOut;
    cw.focusIn = ui->btnFocusIn;
    cw.focusOut = ui->btnFocusOut;
    cw.sliderZoomSpeed = ui->sliderZoomSpeed;
    cw.spinZoomSpeed = ui->spinZoomSpeed;
    cw.callPreset = ui->btnCallPreset;
    cw.setPreset = ui->btnSetPreset;
    cw.delPreset = ui->btnDelPreset;
    cw.ptzReset = ui->btnPtzReset;
    cw.checkDigitalZoom = ui->checkDigitalZoom;
    cw.checkAutoZoom = ui->checkAutoZoom;
    cw.checkCaptureUpload = ui->checkCaptureUpload;
    cw.checkPosReset = ui->checkPosReset;
    cw.btnWiperStart = ui->btnWiperStart;
    cw.btnWiperStop = ui->btnWiperStop;
    cw.btnWiperLeft = ui->btnWiperLeft;
    cw.btnWiperRight = ui->btnWiperRight;
    cw.btnWiperZeroCalib = ui->btnWiperZeroCalib;
    cw.btnWiperMode = ui->btnWiperMode;
    cw.btnWiperSilent = ui->btnWiperSilent;
    cw.editWiperRunCurrent = ui->editWiperRunCurrent;
    cw.editWiperHoldCurrent = ui->editWiperHoldCurrent;
    cw.editWiperHoldDelay = ui->editWiperHoldDelay;
    cw.editMotorMode = ui->editMotorMode;
    cw.editMotorRunCurrent = ui->editMotorRunCurrent;
    cw.editMotorHoldCurrent = ui->editMotorHoldCurrent;
    cw.editMotorHoldDelay = ui->editMotorHoldDelay;
    cw.statusbar = ui->statusbar;
    m_controlService->setup(cw, m_cfg, m_presenter,
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

    //============================================================================
    // 附加功能开关：设备配置变化时同步持久化到设备树
    //============================================================================
    connect(m_presenter, &MainPresenter::deviceConfigChanged, this,
            [this](const QString& deviceId, const DeviceConfig& cfg) {
        m_deviceTree->setConfigForId(deviceId, cfg);
    });

    // 设备切换后：刷新设备树当前焦点标记 + 按新设备协议刷新电机按钮/电流项
    connect(m_presenter, &MainPresenter::currentDeviceChanged, this,
            [this](const QString& deviceId) {
        m_deviceTree->setCurrentDevice(deviceId);
        m_controlService->updateMotorButtons();
        m_videoGrid->setActiveDevice(deviceId);
    });

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
    }
    m_controlService->updateMotorButtons();

    // 启动时按树序初始化设备编号并同步到视频格角标
    refreshDeviceLabelsAndActive();

    // ===== TEMP DEBUG: env LSS_AUTOPROPS=1 自动打开设备属性复现崩溃 =====
    if (qEnvironmentVariableIsSet("LSS_AUTOPROPS")) {
        QTimer::singleShot(6000, this, [this]() {
            const QList<QString> ids = m_deviceTree->allDeviceIds();
            for (const QString& deviceId : ids) {
                if (deviceId.isEmpty()) continue;
                const DeviceEntry entry = m_deviceTree->entryForId(deviceId);
                DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
                DeviceConfig cfg = ctx ? ctx->deviceConfig() : entry.config;
                qWarning() << "[AUTOPROPS] open props for" << entry.ip
                           << "cfg.targetRefMap.size()=" << cfg.targetRefMap.size();
                DevicePropertiesDialog dlg(entry.ip, &cfg, this);
                dlg.exec();
            }
        });
    }
}

//============================================================================
// 析构函数：释放 UI 资源
// 子模块对象 (m_client, m_cfg, m_device, m_rtsp, m_mapWidget)
// 均以 MainWindow 为父对象，由 Qt 对象树自动析构
// 析构前停止 RTSP 线程：设置停止标志后 FFmpeg 中断回调会使其快速返回
//============================================================================
MainWindow::~MainWindow()
{
    // 0. 滚轮重定向过滤器为 MainWindow 子对象，随对象树析构自动清理。

    // 1. 移除 MainWindow 作为过滤器安装到子控件上的 eventFilter
    if (ui && ui->titleBar) ui->titleBar->removeEventFilter(this);
    if (m_mapOverlay)       m_mapOverlay->removeEventFilter(this);
    if (m_pipTitle)         m_pipTitle->removeEventFilter(this);

    // 2. 断开所有进出 MainWindow 的信号连接，防止析构期间回调
    disconnect(this, nullptr, nullptr, nullptr);
    disconnect(nullptr, nullptr, this, nullptr);

    // 3. 清空视窗网格绑定（widget 随 VideoGridWidget 析构自动销毁）
    if (m_videoGrid) {
        const auto keys = m_videoGrid->boundDeviceIds();
        for (const QString& id : keys)
            m_videoGrid->unbindDevice(id);
    }

    // 4. 关闭所有设备（停止 RTSP 线程、断开 TCP、取消自动重连）
    DeviceManager::instance().removeAllDevices();

    // 4. 不手动 delete 服务/m_pipDialog —— 它们是 QObject 子对象，
    //    由 ~QMainWindow() 按构造逆序自动销毁。
    //    delete ui 仅释放 Ui 结构体（不含 QWidget 生命周期）。
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
    QHeaderView* identifyHeader = ui->tableIdentify->horizontalHeader();
    identifyHeader->setStretchLastSection(false);
    identifyHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    identifyHeader->setSectionResizeMode(1, QHeaderView::Fixed);
    identifyHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    identifyHeader->setSectionResizeMode(3, QHeaderView::Stretch);
    identifyHeader->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->tableIdentify->setColumnWidth(0, 40);
    ui->tableIdentify->setColumnWidth(1, 80);
    ui->tableIdentify->setColumnWidth(2, 70);

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

        // 滚轮只作用于控制面板滚动条：悬停在输入控件（Spin/Edit/Combo/Slider）上
        // 时滚动面板，不改变控件数值。
        WheelRedirectFilter::install(ui->scrollAreaControl, this);
    }
}

//============================================================================
// setupInputValidators - 为数值输入框安装 QValidator
// 目的：类型期即拦截字母/符号，避免非法输入被静默解析为 0 或钳到下限。
// 注意：QIntValidator/QDoubleValidator 允许中间态为空串，提交期仍需复校验。
//============================================================================
void MainWindow::setupInputValidators()
{
    // 双精度输入：统一用 C locale（小数点 '.'）且禁用科学计数法
    const auto makeDoubleValidator = [](double lo, double hi, int decimals, QWidget* parent) {
        auto* v = new QDoubleValidator(lo, hi, decimals, parent);
        v->setNotation(QDoubleValidator::StandardNotation);
        v->setLocale(QLocale::c());
        return v;
    };

    // 云台角度：方位 ±360°、俯仰 ±90°
    ui->editTargetPan->setValidator(makeDoubleValidator(-360.0, 360.0, 2, ui->editTargetPan));
    ui->editTargetTilt->setValidator(makeDoubleValidator(-90.0, 90.0, 2, ui->editTargetTilt));

    // 经纬度：十进制度 + 可选 N/S/E/W 后缀（大小写不敏感）
    const QRegularExpression coordRe(QStringLiteral("^[+-]?\\d{0,3}(\\.\\d{0,7})?[NnSsEeWw]?$"));
    for (QLineEdit* le : {ui->editTargetLat, ui->editTargetLon, ui->editSetLat, ui->editSetLon}) {
        le->setValidator(new QRegularExpressionValidator(coordRe, le));
    }

    // 高度（米）
    ui->editTargetAlt->setValidator(makeDoubleValidator(-10000.0, 10000.0, 2, ui->editTargetAlt));
    ui->editSetHeight->setValidator(makeDoubleValidator(-10000.0, 10000.0, 2, ui->editSetHeight));

    // 雨刷电流：运行电流范围随协议动态调整（见 updateMotorButtons），此处先给默认 0-2000
    ui->editWiperRunCurrent->setValidator(new QIntValidator(0, 2000, ui->editWiperRunCurrent));
    ui->editWiperHoldCurrent->setValidator(new QIntValidator(1, 31, ui->editWiperHoldCurrent));
    ui->editWiperHoldDelay->setValidator(new QIntValidator(0, 15, ui->editWiperHoldDelay));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_videoGrid && m_videoGrid->hasFocus()) {
        m_videoGrid->clearFocus();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::refreshDeviceLabelsAndActive()
{
    if (!m_deviceTree || !m_videoGrid) return;
    m_deviceTree->renumberDevices();
    const QList<QString> ids = m_deviceTree->allDeviceIds();
    for (const QString& id : ids) {
        const DeviceEntry e = m_deviceTree->entryForId(id);
        const int number = m_deviceTree->deviceNumberForId(id);
        const QString name = e.name.isEmpty() ? e.ip : e.name;
        m_videoGrid->setDeviceLabel(id, number, name);
    }
    m_videoGrid->setActiveDevice(m_presenter->currentDeviceId());
}

//============================================================================
// onRtspOpened - RTSP 视频流成功打开
//============================================================================
void MainWindow::onRtspOpened(const QString& deviceId)
{
    m_deviceTree->setVideoConnected(deviceId, true);
    // 视频就绪后不再在画面上叠加状态文本（清除“RTSP 未连接”占位/状态）
    if (auto vw = m_videoGrid->getWidget(deviceId))
        vw->clearStatusText();
    ui->statusbar->showMessage(QString::fromUtf8("RTSP 视频已连接"), 3000);
}

//============================================================================
// onRtspError - RTSP 视频流错误处理
//============================================================================
void MainWindow::onRtspError(const QString& deviceId, const QString &msg)
{
    QString statusMsg = msg.isEmpty()
        ? QString::fromUtf8("RTSP 断开，正在重连...")
        : QString::fromUtf8("RTSP 重连失败，继续重试...");
    m_deviceTree->setVideoConnected(deviceId, false);
    if (auto vw = m_videoGrid->getWidget(deviceId)) {
        vw->clearFrame();
        vw->setStatusText(statusMsg);
    }
    if (m_presenter->isVideoStreamRunning()) {
        ui->statusbar->showMessage(statusMsg);
    } else {
        ui->statusbar->showMessage(msg);
    }
}

//============================================================================
// onRtspStats - RTSP 链路健康度更新（可观测性）
// 仅当链路异常（已连接但不健康，或连续失败）时提示，避免刷屏。
//============================================================================
void MainWindow::onRtspStats(const QString& deviceId, const RtspThread::Stats& stats)
{
    Q_UNUSED(deviceId);
    if (stats.connected && !stats.healthy) {
        ui->statusbar->showMessage(
            QString::fromUtf8("RTSP 链路异常，已重连 %1 次，正在恢复...").arg(stats.reconnectCount));
    } else if (!stats.connected && stats.consecutiveFailures >= 3) {
        ui->statusbar->showMessage(
            QString::fromUtf8("RTSP 连接失败 %1 次，持续重试中...").arg(stats.consecutiveFailures));
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
//============================================================================
void MainWindow::onDeviceConnected(const QString& deviceId)
{
    m_deviceTree->setDeviceConnected(deviceId, true);
    ui->statusbar->showMessage(QString::fromUtf8("已连接到设备"), 3000);
}

//============================================================================
// onDeviceDisconnected - 设备断开回调
//============================================================================
void MainWindow::onDeviceDisconnected(const QString& deviceId)
{
    m_deviceTree->setDeviceConnected(deviceId, false);
    // 视窗显示 RTSP 连接状态（设备断开即视频断开）
    // 状态提示仅经状态栏，不在视频画面上叠加文字
    ui->statusbar->showMessage(QString::fromUtf8("设备已断开"), 3000);
}

//============================================================================
// onErrorOccurred - 连接错误处理
//============================================================================
void MainWindow::onErrorOccurred(const QString& errorMsg)
{
    ui->statusbar->showMessage(QString::fromUtf8("连接错误，%1").arg(errorMsg), 3000);
    QMessageBox::warning(this, QString::fromUtf8("连接错误"), errorMsg);
}

//============================================================================
// onAckReceived - 已迁移至 MainPresenter::showAck（经 EventBus 回调）
//============================================================================

//============================================================================


//============================================================================
// requireConnected / requireMotorReady（实现在 mainwindow_imainview.cpp）
//============================================================================

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


void MainWindow::onDeviceReconnecting(int attempt, int maxRetries) {
    Q_UNUSED(maxRetries);
    ui->statusbar->showMessage(QString::fromUtf8("断开，正在重连 %1 次...").arg(attempt));
}

void MainWindow::onDeviceReconnectFailed() {
    ui->statusbar->showMessage(QString::fromUtf8("重连 10 次失败，请检查设备连接"), 5000);
}