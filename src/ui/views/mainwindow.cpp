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

#include "ui/views/settingsdialog.h"
#include "infrastructure/rtspthread.h"
#include "ui/views/videowidget.h"
#include "ui/views/mapwidget.h"
#include <QScreen>
#include "ui/views/cmdlogdialog.h"
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
#include <QRadioButton>

#include <QtMath>



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

    // MapWidget 工具栏信号 → MainWindow
    connect(m_mapWidget, &MapWidget::miniRequested,
            this, [this]() { toggleMapMode(); });
    connect(m_mapWidget, &MapWidget::closeRequested,
            this, [this]() { toggleMap(); });
    connect(m_mapWidget, &MapWidget::enlargeRequested,
            this, [this]() { toggleMapMode(); });
    connect(ui->btnMapToggle, &QPushButton::clicked,
            this, [this]() { toggleMap(); });

    // 首次布局
    updateMapLayout();

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
    // 云台八方向控制 (基于 Pelco-D 协议)
    // 按下按钮 → 发送持续转动指令；释放按钮 → 发送停止指令
    // 八个按钮分别对应 Up/Down/Left/Right 及四个对角线方向
    //============================================================================
    auto connectPtzBtn = [this](QPushButton* btn, PtzDir dir) {
        connect(btn, &QPushButton::pressed, this, [this, dir]() { if (!requireConnected()) return; m_presenter->ptzMove(static_cast<int>(dir)); });
        connect(btn, &QPushButton::released, this, [this]() { if (!m_presenter->isDeviceConnected()) return; m_presenter->ptzStop(); });
    };

    connectPtzBtn(ui->btnPtzUp, PtzDir::Up);
    connectPtzBtn(ui->btnPtzDown, PtzDir::Down);
    connectPtzBtn(ui->btnPtzLeft, PtzDir::Left);
    connectPtzBtn(ui->btnPtzRight, PtzDir::Right);
    connectPtzBtn(ui->btnPtzTopLeft, PtzDir::UpLeft);
    connectPtzBtn(ui->btnPtzTopRight, PtzDir::UpRight);
    connectPtzBtn(ui->btnPtzBottomLeft, PtzDir::DownLeft);
    connectPtzBtn(ui->btnPtzBottomRight, PtzDir::DownRight);

    //============================================================================
    // 云台速度控制
    // 滑块与数值输入框双向绑定，值改变时保存到配置持久化
    // 水平与垂直速度使用相同的数值
    //============================================================================
    ui->sliderSpeed->setValue(m_cfg->ptz().panSpeed);
    ui->spinSpeed->setValue(m_cfg->ptz().panSpeed);

    connect(ui->sliderSpeed, &QSlider::valueChanged, ui->spinSpeed, &QSpinBox::setValue);
    connect(ui->spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderSpeed, &QSlider::setValue);
    connect(ui->spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->ptz().panSpeed = static_cast<quint8>(val);
        m_cfg->ptz().tiltSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    //============================================================================
    // 镜头控制 (Zoom 变倍 / Focus 调焦)
    // 按下按钮 → 持续变倍/调焦；释放按钮 → 停止
    // op 值: 0=ZoomIn, 1=ZoomOut, 2=FocusIn, 3=FocusOut
    // 镜头目标（可见光/红外）由 Presenter 根据显示模式自动判断
    //============================================================================
    auto connectLensBtn = [this](QPushButton* btn, int op) {
        connect(btn, &QPushButton::pressed, this, [this, op]() {
            if (!requireConnected()) return;
            m_presenter->lensMove(op);
        });
        connect(btn, &QPushButton::released, this, [this]() { if (!m_presenter->isDeviceConnected()) return; m_presenter->lensStop(); });
    };

    connectLensBtn(ui->btnZoomIn, 0);
    connectLensBtn(ui->btnZoomOut, 1);
    connectLensBtn(ui->btnFocusIn, 2);
    connectLensBtn(ui->btnFocusOut, 3);

    //============================================================================
    // 镜头速度控制 (变倍速度 / 调焦速度)
    // 滑块与数值输入框双向绑定，值改变时自动保存配置
    //============================================================================
    ui->sliderZoomSpeed->setValue(m_cfg->lens().zoomSpeed);
    ui->spinZoomSpeed->setValue(m_cfg->lens().zoomSpeed);
    connect(ui->sliderZoomSpeed, &QSlider::valueChanged, ui->spinZoomSpeed, &QSpinBox::setValue);
    connect(ui->spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderZoomSpeed, &QSlider::setValue);
    connect(ui->spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->lens().zoomSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    //============================================================================
    // 预置位控制 (调用/设置/删除)
    // 通过 spinPreset 选择预置位编号，调用 DeviceController 中的协议封装
    //============================================================================
    connect(ui->btnCallPreset, &QPushButton::clicked, this, [this]() {
        m_presenter->on_btnCallPreset_clicked();
    });
    connect(ui->btnSetPreset, &QPushButton::clicked, this, [this]() {
        m_presenter->on_btnSetPreset_clicked();
    });
    connect(ui->btnDelPreset, &QPushButton::clicked, this, [this]() {
        m_presenter->on_btnDelPreset_clicked();
    });

    //============================================================================
    // 附加功能开关 (数字变倍 / 自动变焦 / 抓拍上传 / 位置归零)
    // 每个 CheckBox 直连对应的设备指令
    //============================================================================
    connect(ui->checkDigitalZoom, &QCheckBox::toggled, this, [this](bool checked) {
        m_presenter->onCheckDigitalZoomToggled(checked);
    });
    connect(ui->checkAutoZoom, &QCheckBox::toggled, this, [this](bool checked) {
        m_presenter->onCheckAutoZoomToggled(checked);
    });
    connect(ui->checkCaptureUpload, &QCheckBox::toggled, this, [this](bool checked) {
        m_presenter->onCheckCaptureUploadToggled(checked);
    });
    connect(ui->checkPosReset, &QCheckBox::toggled, this, [this](bool checked) {
        m_presenter->onCheckPosResetToggled(checked);
    });
    connect(ui->btnWiperStart, &QPushButton::clicked, this, [this]() {
        m_presenter->onWiperStart();
    });
    connect(ui->btnWiperStop, &QPushButton::clicked, this, [this]() {
        m_presenter->onWiperStop();
    });
    connect(m_presenter, &MainPresenter::motorModeChanged, this, [this](bool isManual) {
        ui->statWiperStatus->setText(isManual ? "手动" : "自动");
    });
    connect(m_presenter, &MainPresenter::motorSerialErrorOccurred, this, [this](const QString& msg) {
        ui->statWiperStatus->setText("故障");
        qWarning() << "电机串口错误:" << msg;
    });
    connect(ui->btnWiperLeft, &QPushButton::pressed, this, [this]() {
        m_presenter->onWiperJogLeft();
    });
    connect(ui->btnWiperLeft, &QPushButton::released, this, [this]() {
        m_presenter->onWiperJogStop();
    });
    connect(ui->btnWiperRight, &QPushButton::pressed, this, [this]() {
        m_presenter->onWiperJogRight();
    });
    connect(ui->btnWiperRight, &QPushButton::released, this, [this]() {
        m_presenter->onWiperJogStop();
    });
    connect(ui->btnWiperZeroCalib, &QPushButton::clicked, this, [this]() {
        m_presenter->onWiperZeroCalib();
    });
    connect(ui->btnWiperMode, &QPushButton::clicked, this, [this]() {
        m_presenter->onWiperMode();
    });
    connect(ui->btnWiperSilent, &QPushButton::clicked, this, [this]() {
        m_presenter->onWiperSilent();
    });
    connect(m_presenter, &MainPresenter::motorSilentChanged, this, [this](bool isSilent) {
        ui->btnWiperSilent->setText(isSilent ? "狂暴模式" : "静音模式");
        ui->statusbar->showMessage(isSilent ? "电机已切换为：静音模式 (StealthChop)" : "电机已切换为：狂暴模式 (SpreadCycle)", 3000);
    });
    connect(ui->editWiperCurrent, &QLineEdit::editingFinished, this, [this]() {
        m_presenter->onWiperCurrentSet();
    });

    connect(ui->btnPtzReset, &QPushButton::clicked, this, [this]() {
        m_presenter->on_btnPtzReset_clicked();
    });

    //============================================================================
    // 指令日志窗口
    // 实时显示所有下发给设备的指令内容，方便调试与协议分析
    //============================================================================
    m_logDialog = new CmdLogDialog(this);
    connect(m_presenter, &MainPresenter::commandSentToLog, m_logDialog, &CmdLogDialog::appendLog);

    //============================================================================
    // 系统托盘
    // 关闭窗口时最小化到托盘，右键菜单可退出程序
    //============================================================================
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/qss/logo.ico")));
    m_trayIcon->setToolTip(QStringLiteral("LSS视频管理客户端"));

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(QStringLiteral("显示主窗口"), this, &MainWindow::onTrayShow);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(QStringLiteral("退出"), this, &MainWindow::onTrayExit);

    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    m_trayIcon->show();

    for (auto *cb : findChildren<QComboBox *>()) {
        cb->setFocusPolicy(Qt::StrongFocus);
        cb->installEventFilter(this);
    }
    updateMotorButtons();
}

//============================================================================
// 析构函数：释放 UI 资源
// 子模块对象 (m_client, m_cfg, m_device, m_rtsp, m_mapWidget)
// 均以 MainWindow 为父对象，由 Qt 对象树自动析构
// 析构前停止 RTSP 线程：设置停止标志后 FFmpeg 中断回调会使其快速返回
//============================================================================
MainWindow::~MainWindow()
{
    // Make sure all devices are properly stopped and threads are terminated
    // This prevents background RTSP threads from causing Heap Corruption on exit
    DeviceManager::instance()->removeAllDevices();

    delete m_pipDialog;
    delete ui;
}

//============================================================================
// 标题栏按钮
//============================================================================

void MainWindow::on_btnMenu_Min_clicked()
{
    showMinimized();
}

void MainWindow::on_btnMenu_Max_clicked()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void MainWindow::on_btnMenu_Close_clicked()
{
    auto action = m_cfg->closeAction();
    if (action == ConfigManager::Exit) {
        m_trayIcon->hide();
        qApp->quit();
        return;
    }
    if (action == ConfigManager::Minimize) {
        hide();
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("关闭提示"));
    dlg.setFixedSize(300, 160);
    dlg.setWindowFlags((dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint));

    auto *layout = new QVBoxLayout(&dlg);

    // Radio 按钮行：左对齐退出程序，右对齐最小化到托盘
    auto *radioLayout = new QHBoxLayout();
    auto *radioExit = new QRadioButton(QStringLiteral("退出程序"), &dlg);
    auto *radioMin = new QRadioButton(QStringLiteral("最小化到托盘"), &dlg);
    radioMin->setChecked(true);
    radioLayout->addWidget(radioExit);
    radioLayout->addStretch();
    radioLayout->addWidget(radioMin);
    layout->addLayout(radioLayout);

    // 底部行：记住选择（左）+ 确认（右）
    auto *bottomLayout = new QHBoxLayout();
    auto *cbRemember = new QCheckBox(QStringLiteral("记住本次选择"), &dlg);
    bottomLayout->addWidget(cbRemember);
    bottomLayout->addStretch();
    auto *btnConfirm = new QPushButton(QStringLiteral("确认"), &dlg);
    btnConfirm->setFixedWidth(80);
    bottomLayout->addWidget(btnConfirm);
    layout->addLayout(bottomLayout);

    connect(btnConfirm, &QPushButton::clicked, this, [this, &dlg, radioExit, cbRemember]() {
        if (cbRemember->isChecked()) {
            m_cfg->setCloseAction(radioExit->isChecked()
                ? ConfigManager::Exit : ConfigManager::Minimize);
            m_cfg->save();
        }
        if (radioExit->isChecked()) {
            m_trayIcon->hide();
            qApp->quit();
        } else {
            hide();
        }
        dlg.close();
    });

    dlg.exec();
}

//============================================================================
// 系统托盘
//============================================================================

void MainWindow::closeEvent(QCloseEvent *event)
{
    auto action = m_cfg->closeAction();
    if (action == ConfigManager::Exit) {
        m_trayIcon->hide();
        qApp->quit();
        event->accept();
        return;
    }
    if (action == ConfigManager::Minimize) {
        hide();
        event->ignore();
        return;
    }
    if (m_trayIcon->isVisible()) {
        hide();
        m_trayIcon->showMessage(QStringLiteral("LSS Video Manager"),
                                QStringLiteral("程序已最小化到系统托盘"),
                                QSystemTrayIcon::Information, 2000);
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
        onTrayShow();
}

void MainWindow::onTrayShow()
{
    showNormal();
    activateWindow();
    raise();
}

void MainWindow::onTrayExit()
{
    m_trayIcon->hide();
    m_presenter->closeVideoStream();
    if (auto vw = m_videoGrid->getWidget(m_presenter->currentDeviceId())) vw->clearFrame();
    if (m_presenter->isDeviceConnected())
        m_presenter->disconnectDevice();
    qApp->quit();
}

//============================================================================
// 导航按钮
//============================================================================

void MainWindow::on_btnNavMonitor_clicked()  { /* 当前页面 */ }
void MainWindow::on_btnNavPlayback_clicked() { /* 预留 */ }
void MainWindow::on_btnNavLog_clicked()      {
    if (m_logDialog->isVisible()) {
        m_logDialog->hide();
    } else {
        m_logDialog->show();
        m_logDialog->raise();
        m_logDialog->activateWindow();
    }
}
void MainWindow::on_btnNavSettings_clicked() {
    SettingsDialog dlg(m_cfg, this);
    dlg.exec();

    // 电机协议变更后重新打开串口
    m_presenter->applyMotorChannel();
    updateMotorButtons();

    // 重启 PTZ 转发服务
    m_presenter->initPtzForwarder();
}

//============================================================================
// changeEvent - 窗口状态变化时更新最大化按钮图标
//============================================================================

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        bool max = isMaximized();
        ui->btnMenu_Max->setIcon(QIcon(max
            ? QStringLiteral(":/qss/blacksoft/restore.png")
            : QStringLiteral(":/qss/blacksoft/maximize.png")));
        ui->btnMenu_Max->setToolTip(max
            ? QString::fromUtf8("窗口化")
            : QString::fromUtf8("最大化"));
    }
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
    if (!m_presenter->isDeviceConnected()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("提示"));
        msgBox.setText(QStringLiteral("请连接设备"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
        msgBox.exec();
        return false;
    }
    return true;
}

// 检查电机是否就绪
// MODBUS-RTU 协议时需串口已打开，Pelco-D 直发即可
bool MainWindow::requireMotorReady()
{
    if (m_cfg->motorProtocol() == "MODBUS-RTU") {
        if (m_cfg->motorCommandChannel() == "串口" && !m_presenter->isMotorSerialOpen()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("提示"));
            msgBox.setText(QStringLiteral("电机串口未打开，请在设置中配置"));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
            msgBox.exec();
            return false;
        }
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        if (!m_presenter->isMotorTcpOpen()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("提示"));
            msgBox.setText(QStringLiteral("电机 TCP 正在连接或连接失败，请检查配置"));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
            // 这里我们允许它尝试去重连，不直接 return false
            // 但是如果是要强制的话可以 return false; 
            // 我们在 DeviceController 里面已经有 sendMotorTcpV4 时如果断开会自动尝试重连一次
        }
    }
    return true;
}

// 更新电机控制按钮状态
void MainWindow::updateMotorButtons()
{
    bool isModbus = (m_cfg->motorProtocol() == "MODBUS-RTU");
    bool isTcp = (m_cfg->motorProtocol() == "STM32-TCP-V4.0");
    bool isPelco = (m_cfg->motorProtocol() == "Pelco-D");
    
    // 只有 Modbus 和 TCP 全功能可用，Pelco-D 仅允许雨刷
    
    bool othersEnabled = !isPelco;
    ui->btnWiperLeft->setEnabled(othersEnabled);
    ui->btnWiperRight->setEnabled(othersEnabled);
    ui->btnWiperZeroCalib->setEnabled(othersEnabled);
    ui->btnWiperMode->setEnabled(othersEnabled);
    
    // 狂暴/静音模式仅在 STM32-TCP-V4.0 下有效，或者如果您希望 Modbus 也有预留，可以调整
    // 根据文档，action 6/7 属于 V4.0 TCP 接口
    ui->btnWiperSilent->setEnabled(isTcp);

    if (isModbus && m_presenter->isMotorSerialOpen()) {
        m_presenter->checkMotorMode();
    }
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
    updateMapLayout();
}

//============================================================================
// toggleMap - 切换地图显示/隐藏
// 由 m_btnMap 触发
//============================================================================
void MainWindow::toggleMap()
{
    m_mapVisible = !m_mapVisible;
    ui->btnMapToggle->setChecked(m_mapVisible);
    if (m_mapVisible) {
        m_mapExpanded = false;
        updateMapLayout();
    } else {
        m_pipDialog->hide();
        if (m_videoGrid->parent() != ui->widgetDisplay) {
            m_videoGrid->setParent(ui->widgetDisplay);
            ui->verticalLayout_display->addWidget(m_videoGrid);
        }
        m_mapContainer->setVisible(false);
    }
}

//============================================================================
// toggleMapMode - 切换迷你/全屏模式
// 迷你模式下单击地图触发展开；全屏模式下点 ✕ 收回
//============================================================================
void MainWindow::toggleMapMode()
{
    m_mapExpanded = !m_mapExpanded;
    updateMapLayout();
}

//============================================================================
// updateMapLayout - 三模式布局
//   地图隐藏    → videoWidget 填满 widgetDisplay
//   迷你模式    → videoWidget 全屏 + 280 圆形浮层
//   全屏/大地图  → mapWidget 填满 widgetDisplay + 独立 PiP 对话框
//============================================================================
void MainWindow::updateMapLayout()
{
    QSize ps = ui->widgetDisplay->size();
    if (ps.isEmpty()) return;

    if (!m_mapVisible) {
        if (m_videoGrid->parent() != ui->widgetDisplay) {
            m_videoGrid->setParent(ui->widgetDisplay);
            ui->verticalLayout_display->addWidget(m_videoGrid);
            m_videoGrid->setVisible(true);
        }
        m_mapContainer->setVisible(false);
        m_pipDialog->hide();
        return;
    }

    m_mapContainer->setVisible(true);

    if (m_mapExpanded) {
        // 大地图：地图填满显示区
        m_mapContainer->setGeometry(0, 0, ps.width(), ps.height());
        m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, false);
        m_mapContainer->clearMask();
        m_mapWidget->setGeometry(0, 0, ps.width(), ps.height());
        m_mapWidget->setCircularClip(false);
        m_mapOverlay->setVisible(false);

        // 视频移至独立 PiP 对话框
        m_videoGrid->setParent(m_pipDialog);
        m_pipDialog->layout()->addWidget(m_videoGrid);
        m_pipPos = QPoint(8, ps.height() - 240 - 8);
        m_pipDialog->move(m_pipPos);
        m_pipDialog->show();
        m_videoGrid->setVisible(true);
    } else {
        // 迷你模式
        if (m_videoGrid->parent() != ui->widgetDisplay) {
            m_videoGrid->setParent(ui->widgetDisplay);
            ui->verticalLayout_display->addWidget(m_videoGrid);
            m_videoGrid->setVisible(true);
        }
        m_pipDialog->hide();

        m_mapContainer->setGeometry(m_miniMapPos.x(), m_miniMapPos.y(), 280, 280);
        m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);
        m_mapContainer->setMask(QRegion(0, 0, 280, 280, QRegion::Ellipse));
        m_mapWidget->setGeometry(0, 0, 280, 280);
        double lat = GeoCalculator::parseCoord(ui->statLatitude->text());
        double lon = GeoCalculator::parseCoord(ui->statLongitude->text());
        if (lat != 0 || lon != 0)
            m_mapWidget->setCircularClip(true, lat, lon, 12);
        else
            m_mapWidget->setCircularClip(true);
        m_mapOverlay->setGeometry(0, 0, 280, 280);
        m_mapOverlay->setVisible(true);
    }
    m_mapContainer->raise();
}

//============================================================================
// eventFilter - 全局事件过滤
//   QComboBox：拦截滚轮，仅下拉列表展开时才允许滚轮切换
//   m_mapOverlay：单击＝展开，拖拽＝移动位置
//   m_pipTitle：拖拽移动 PiP 对话框位置
//============================================================================
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // QComboBox 滚轮拦截：未展开时忽略滚轮事件
    if (event->type() == QEvent::Wheel) {
        auto *cb = qobject_cast<QComboBox*>(obj);
        if (cb && !cb->view()->isVisible()) {
            return true;  // 吞掉滚轮事件
        }
    }

    if (obj == m_mapOverlay) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStart = me->pos();
                m_dragging = false;
            }
            return true;
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                QPoint delta = me->pos() - m_dragStart;
                if (delta.manhattanLength() > 5) {
                    m_dragging = true;
                    m_miniMapPos = m_mapContainer->pos() + delta;
                    m_mapContainer->move(m_miniMapPos);
                }
            }
            return true;
        }
        case QEvent::MouseButtonRelease: {
            m_dragging = false;
            return true;
        }
        case QEvent::MouseButtonDblClick: {
            toggleMapMode();
            return true;
        }
        default:
            break;
        }
    }

    if (obj == m_pipTitle) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_pipDragStart = me->globalPosition().toPoint();
                m_pipTitle->setCursor(Qt::ClosedHandCursor);
            }
            return true;
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                m_pipPos = m_pipDialog->pos() + (me->globalPosition().toPoint() - m_pipDragStart);
                m_pipDialog->move(m_pipPos);
                m_pipDragStart = me->globalPosition().toPoint();
            }
            return true;
        }
        case QEvent::MouseButtonRelease: {
            m_pipTitle->setCursor(Qt::OpenHandCursor);
            return true;
        }
        case QEvent::MouseButtonDblClick: {
            // 双击标题栏：恢复视频到主显示区 + 显示迷你地图
            toggleMapMode();
            return true;
        }
        default:
            break;
        }
    }

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

VideoWidget* MainWindow::videoWidget(const QString& deviceId)
{
    return m_videoGrid->bindDevice(deviceId);
}

void MainWindow::repaintVideoGrid()
{
    m_videoGrid->repaint();
}

MapWidget* MainWindow::mapWidget()
{
    return m_mapWidget;
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
