//============================================================================
// mainwindow.cpp - T-JSON 主窗口实现
// 包含主窗口的构造/析构、UI 样式初始化、信号-槽连接，
// 以及完整的 JSON 帧解析、状态更新、地图坐标转换、云台镜头控制逻辑。
//============================================================================
#include "mainwindow.h"
#include "core/GeoCalculator.h"
#include "ui_mainwindow.h"
#include <QAbstractButton>
#include <QLineEdit>
#include "ui/components/VideoGridWidget.h"
#include "ui/components/DeviceTreeWidget.h"

#include "settingsdialog.h"
#include "rtspthread.h"
#include "videowidget.h"
#include "mapwidget.h"
#include <QScreen>
#include "cmdlogdialog.h"
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
    , m_updatingFromDevice(false)            // 防递归更新初始关闭
    , m_currentVisZoom(1.0)                  // 默认可见光倍率 1.0
    , m_currentIrZoom(1.0)                   // 默认红外倍率 1.0
    , m_currentTilt(0.0)                     // 默认俯仰角 0
    , m_currentPipShow(0)                    // 默认显示模式：大图可见光
    , m_workModeInitialized(false)
    , m_currentResX(m_cfg->cam().visResX)    // 默认可见光分辨率
    , m_currentResY(m_cfg->cam().visResY)
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
    m_videoGrid->bindDevice("default_device");


    
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

    // 根据配置自动初始化连接
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        m_presenter->motorController()->openMotorSerial(m_cfg->motorComPort());
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_presenter->motorController()->openMotorTcp();
    }

    connect(m_presenter->ptzForwarder(), &PtzForwarder::ptzAnglesUpdated, this, [this](double, double) {
        // 转台角度已改用 8089 端口 JSON 数据更新
    });

    // PTZ Forwarder start（延迟到事件循环启动后）
    QTimer::singleShot(0, this, [this]() {
        if (m_cfg->serialServerEnabled()) {
            m_presenter->ptzForwarder()->start(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
            m_presenter->ptzForwarder()->setOffsets(m_cfg->ptzPanOffset(), m_cfg->ptzTiltOffset());
        }
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
    if (auto vw = m_videoGrid->getWidget("default_device")) vw->setSelectionEnabled(wm == 3 || wm == 4);

    //============================================================================
    // RTSP 视频流信号连接
    // RtspThread 在工作线程中拉流解码，通过信号将帧数据传回主线程
    // VideoWidget 的 selectionFinished 信号用于框选跟踪
    //============================================================================
    connect(m_presenter->videoStream(), &RtspThread::frameReady, this, &MainWindow::onRtspFrame);
    connect(m_presenter->videoStream(), &RtspThread::streamOpened, this, &MainWindow::onRtspOpened);
    connect(m_presenter->videoStream(), &RtspThread::streamError, this, &MainWindow::onRtspError);
    if (VideoWidget* vw = m_videoGrid->getWidget("default_device")) {
        connect(vw, &VideoWidget::selectionFinished, this, &MainWindow::onVideoSelection);
    }

    //============================================================================
    // T-JSON 协议信号连接
    // TJsonClient 管理 TCP 长连接、心跳保活、JSON 帧收发与自动重连
    //============================================================================
    connect(m_presenter->tcpClient(), &TJsonClient::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(m_presenter->tcpClient(), &TJsonClient::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);
    connect(m_presenter->tcpClient(), &TJsonClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_presenter->tcpClient(), &TJsonClient::jsonReceived, this, &MainWindow::onJsonReceived);
    connect(m_presenter->tcpClient(), &TJsonClient::imageSnapped, this, &MainWindow::onImageSnapped);
    connect(m_presenter->tcpClient(), &TJsonClient::ackReceived, this, &MainWindow::onAckReceived);
    
    // 自动重连信号：每次重连尝试时更新按钮文本与状态栏提示
    connect(m_presenter->tcpClient(), &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        Q_UNUSED(maxRetries);
        ui->btnConnect->setText(QString::fromUtf8("重连中(次数:%1)").arg(attempt));
        ui->btnConnect->setEnabled(false);
        ui->btnConnect->setProperty("state", "reconnecting");
        refreshStyle(ui->btnConnect);
        ui->btnCancelConnect->setVisible(true);
        ui->statusbar->showMessage(QString::fromUtf8("网络波动，正在进行第 %1 次自动探测重连...").arg(attempt));
    });
    // 重连失败：恢复按钮初始状态
    connect(m_presenter->tcpClient(), &TJsonClient::reconnectFailed, this, [this]() {
        ui->btnConnect->setText(QString::fromUtf8("连接设备"));
        ui->btnConnect->setEnabled(true);
        ui->btnConnect->setProperty("state", QVariant());
        refreshStyle(ui->btnConnect);
        ui->btnCancelConnect->setVisible(false);
        ui->statusbar->showMessage(QString::fromUtf8("重连失败，已放弃连接"), 5000);
    });

    //============================================================================
    // 云台八方向控制 (基于 Pelco-D 协议)
    // 按下按钮 → 发送持续转动指令；释放按钮 → 发送停止指令
    // 八个按钮分别对应 Up/Down/Left/Right 及四个对角线方向
    //============================================================================
    auto connectPtzBtn = [this](QPushButton* btn, PtzDir dir) {
        connect(btn, &QPushButton::pressed, this, [this, dir]() { if (!requireConnected()) return; m_presenter->motorController()->ptzMove(dir); });
        connect(btn, &QPushButton::released, this, [this]() { if (!m_presenter->tcpClient()->isConnected()) return; m_presenter->motorController()->ptzStop(); });
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
    // 镜头目标根据显示模式自动判断：PipShow 1/4=红外(target=1)，其余=可见光(target=0)
    //============================================================================
    auto connectLensBtn = [this](QPushButton* btn, int op) {
        connect(btn, &QPushButton::pressed, this, [this, op]() {
            if (!requireConnected()) return;
            int t = (m_currentPipShow == 1 || m_currentPipShow == 4) ? 1 : 0;
            if (op == 0) m_presenter->motorController()->lensZoomIn(t);
            else if (op == 1) m_presenter->motorController()->lensZoomOut(t);
            else if (op == 2) m_presenter->motorController()->lensFocusIn(t);
            else m_presenter->motorController()->lensFocusOut(t);
        });
        connect(btn, &QPushButton::released, this, [this]() { if (!m_presenter->tcpClient()->isConnected()) return; m_presenter->motorController()->lensStop(); });
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
        if (!requireConnected()) return;
        m_presenter->motorController()->callPreset(ui->spinPreset->value());
    });
    connect(ui->btnSetPreset, &QPushButton::clicked, this, [this]() {
        if (!requireConnected()) return;
        m_presenter->motorController()->setPreset(ui->spinPreset->value());
    });
    connect(ui->btnDelPreset, &QPushButton::clicked, this, [this]() {
        if (!requireConnected()) return;
        m_presenter->motorController()->delPreset(ui->spinPreset->value());
    });

    //============================================================================
    // 附加功能开关 (数字变倍 / 自动变焦 / 抓拍上传 / 位置归零)
    // 每个 CheckBox 直连对应的设备指令
    //============================================================================
    connect(ui->checkDigitalZoom, &QCheckBox::toggled, this, [this](bool checked) {
        if (!requireConnected()) { ui->checkDigitalZoom->blockSignals(true); ui->checkDigitalZoom->setChecked(!checked); ui->checkDigitalZoom->blockSignals(false); return; }
        m_lastAckFrameType = FrameType::SetDigitalZoom;
        m_presenter->motorController()->setDigitalZoom(checked);
        m_cfg->setDigitalZoomEnabled(checked);
        m_cfg->save();
    });
    connect(ui->checkAutoZoom, &QCheckBox::toggled, this, [this](bool checked) {
        if (!requireConnected()) { ui->checkAutoZoom->blockSignals(true); ui->checkAutoZoom->setChecked(!checked); ui->checkAutoZoom->blockSignals(false); return; }
        m_lastAckFrameType = FrameType::SetAlgoModel;
        m_presenter->motorController()->setAutoZoom(checked);
        m_cfg->setAutoZoomEnabled(checked);
        m_cfg->save();
    });
    connect(ui->checkCaptureUpload, &QCheckBox::toggled, this, [this](bool checked) {
        if (!requireConnected()) { ui->checkCaptureUpload->blockSignals(true); ui->checkCaptureUpload->setChecked(!checked); ui->checkCaptureUpload->blockSignals(false); return; }
        m_lastAckFrameType = FrameType::SetCaptureState;
        m_presenter->motorController()->setCaptureUpload(checked);
        m_cfg->setCaptureUploadEnabled(checked);
        m_cfg->save();
    });
    connect(ui->checkPosReset, &QCheckBox::toggled, this, [this](bool checked) {
        if (!requireConnected()) { ui->checkPosReset->blockSignals(true); ui->checkPosReset->setChecked(!checked); ui->checkPosReset->blockSignals(false); return; }
        m_lastAckFrameType = FrameType::SetPosReset;
        m_presenter->motorController()->posReset(checked);
        m_cfg->setPosResetEnabled(checked);
        m_cfg->save();
    });
    connect(ui->btnWiperStart, &QPushButton::clicked, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorStart();
    });
    connect(ui->btnWiperStop, &QPushButton::clicked, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorStop();
        QTimer::singleShot(50, this, [this]() {
            m_presenter->motorController()->motorReturnZero();
        });
    });
    connect(m_presenter->motorController(), &DeviceController::motorModeResult, this, [this](bool isManual) {
        ui->statWiperStatus->setText(isManual ? "手动" : "自动");
    });
    connect(m_presenter->motorController(), &DeviceController::motorSerialError, this, [this](const QString& msg) {
        ui->statWiperStatus->setText("故障");
        qWarning() << "电机串口错误:" << msg;
    });
    connect(ui->btnWiperLeft, &QPushButton::pressed, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorJogLeft();
    });
    connect(ui->btnWiperLeft, &QPushButton::released, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorStop();
    });
    connect(ui->btnWiperRight, &QPushButton::pressed, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorJogRight();
    });
    connect(ui->btnWiperRight, &QPushButton::released, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorStop();
    });
    connect(ui->btnWiperZeroCalib, &QPushButton::clicked, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorZeroCalib();
    });
    connect(ui->btnWiperMode, &QPushButton::clicked, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorToggleMode();
        QTimer::singleShot(500, this, [this]() {
            m_presenter->motorController()->motorCheckMode();
        });
    });
    connect(ui->btnWiperSilent, &QPushButton::clicked, this, [this]() {
        if (!requireMotorReady()) return;
        m_presenter->motorController()->motorToggleSilentMode();
    });
    connect(m_presenter->motorController(), &DeviceController::motorSilentResult, this, [this](bool isSilent) {
        ui->btnWiperSilent->setText(isSilent ? "狂暴模式" : "静音模式");
        ui->statusbar->showMessage(isSilent ? "电机已切换为：静音模式 (StealthChop)" : "电机已切换为：狂暴模式 (SpreadCycle)", 3000);
    });
    connect(ui->editWiperCurrent, &QLineEdit::editingFinished, this, [this]() {
        int ma = ui->editWiperCurrent->text().toInt();
        m_presenter->motorController()->motorSetCurrent(ma);
        ui->statusbar->showMessage(QString("正在下发并固化电机电流: %1 mA").arg(ma), 3000);
    });

    connect(ui->btnPtzReset, &QPushButton::clicked, this, [this]() {
        if (!requireConnected()) return;
        m_presenter->motorController()->callPreset(0);
    });

    //============================================================================
    // 指令日志窗口
    // 实时显示所有下发给设备的指令内容，方便调试与协议分析
    //============================================================================
    m_logDialog = new CmdLogDialog(this);
    connect(m_presenter->motorController(), &DeviceController::commandSent, m_logDialog, &CmdLogDialog::appendLog);

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
    disconnect(m_presenter->tcpClient(), nullptr, this, nullptr);
    if (m_presenter->videoStream()) {
        if (auto vw = m_videoGrid->getWidget("default_device")) vw->clearFrame();
        m_presenter->videoStream()->closeStream();
        m_presenter->videoStream()->wait(2000);
    }
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
    if (m_presenter->videoStream()) {
        if (auto vw = m_videoGrid->getWidget("default_device")) vw->clearFrame();
        m_presenter->videoStream()->closeStream();
    }
    if (m_presenter->tcpClient()->isConnected())
        m_presenter->tcpClient()->disconnectDevice();
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
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        m_presenter->motorController()->openMotorSerial(m_cfg->motorComPort());
        m_presenter->motorController()->closeMotorTcp();
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_presenter->motorController()->openMotorTcp();
        m_presenter->motorController()->closeMotorSerial();
    } else {
        m_presenter->motorController()->closeMotorSerial();
        m_presenter->motorController()->closeMotorTcp();
    }
    updateMotorButtons();

    // 重启 PTZ 转发服务
    if (m_cfg->serialServerEnabled()) {
        m_presenter->ptzForwarder()->start(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
    } else {
        // 应该也停止它，但目前没有停止方法。假设 start 足够或者是单次触发。
        // Let's assume PtzForwarder doesn't have stop or it doesn't matter for now.
    }
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
void MainWindow::onRtspFrame(const QImage &frame)
{
    if (VideoWidget* vw = m_videoGrid->getWidget("default_device")) {
        vw->setFrame(frame);
    }
}

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
    if (auto vw = m_videoGrid->getWidget("default_device")) vw->clearFrame();
    if (m_presenter->videoStream()->isRunning()) {
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
void MainWindow::onVideoSelection(int cx, int cy, int pw, int ph)
{
    int wm = ui->comboWorkMode->currentIndex();
    if (wm != 3 && wm != 4) {
        ui->statusbar->showMessage(QString::fromUtf8("仅在点选跟踪或框选跟踪模式下支持框选"), 3000);
        return;
    }

    if (wm == 3) {
        ui->statusbar->showMessage(
            QString::fromUtf8("点选跟踪: 像素中心(%1,%2)")
                .arg(cx).arg(cy));
        if (!requireConnected()) return;
        m_presenter->motorController()->setPointTrack(cx, cy);
    } else {
        ui->statusbar->showMessage(
            QString::fromUtf8("框选跟踪: 像素中心(%1,%2) 宽%3高%4")
                .arg(cx).arg(cy).arg(pw).arg(ph));
        if (!requireConnected()) return;
        m_presenter->motorController()->setBoxTrack(cx, cy, pw, ph);
    }
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

    m_workModeInitialized = false;
    m_displayModeInitialized = false;
    m_algoModelInitialized = false;
    // 连接成功后自动请求一次图像参数，以便 UI 与设备状态同步
    m_presenter->motorController()->queryImageParams();

    // 连接后同步所有缓存开关状态，确保设备与 UI 一致
    m_presenter->motorController()->setDigitalZoom(m_cfg->digitalZoomEnabled());
    m_presenter->motorController()->setAutoZoom(m_cfg->autoZoomEnabled());
    m_presenter->motorController()->setCaptureUpload(m_cfg->captureUploadEnabled());
    m_presenter->motorController()->posReset(m_cfg->posResetEnabled());

    // 启动系统参数定时下发

    // 首次连接设备时自动打开 RTSP，后续不再覆盖用户操作
    if (!m_rtspEverOpened) {
        QString rtspUrl = ui->lineEditRtsp->text().trimmed();
        if (!rtspUrl.isEmpty()) {
            m_rtspEverOpened = true;
            ui->btnVideoConnect->setEnabled(false);
            ui->btnVideoConnect->setText(QString::fromUtf8("连接中..."));
            m_presenter->videoStream()->openStream(rtspUrl);
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
// onAckReceived - 处理设备返回的 ACK 应答
// ACK 状态码:
//   0 = 执行正常, 1 = 包不完整, 2 = 协议内容错误
// SetDigitalZoom/SetCaptureState/SetPosReset 这三个指令设备固定回 1，按成功处理
//============================================================================
void MainWindow::onAckReceived(quint8 statusCode)
{
    if (statusCode == 0) {
        ui->statusbar->showMessage(QString::fromUtf8("[ACK] 指令执行成功"), 3000);
        return;
    }
    if (statusCode == 1) {
        // SetDigitalZoom/SetCaptureState/SetPosReset 设备固定回 1，视为成功
        if (m_lastAckFrameType == FrameType::SetDigitalZoom
            || m_lastAckFrameType == FrameType::SetCaptureState
            || m_lastAckFrameType == FrameType::SetPosReset) {
            ui->statusbar->showMessage(QString::fromUtf8("[ACK] 指令执行成功"), 3000);
            return;
        }
        ui->statusbar->showMessage(QString::fromUtf8("[ACK] 包不完整"), 3000);
        return;
    }
    QString msg;
    switch (statusCode) {
    case 2: msg = QString::fromUtf8("协议内容错误"); break;
    default: msg = QString::fromUtf8("未知状态码: %1").arg(statusCode);
    }
    ui->statusbar->showMessage(QString::fromUtf8("[ACK] %1").arg(msg), 3000);
}

//============================================================================
// onJsonReceived - 收到设备推送的 JSON 数据帧
// 将完整 JSON 文档交由 updateStatusFromJson 进行解析与 UI 刷新
//============================================================================
void MainWindow::onJsonReceived(const QJsonObject& doc)
{
    updateStatusFromJson(doc);
}

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
    if (!m_presenter->tcpClient()->isConnected()) {
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
        if (m_cfg->motorCommandChannel() == "串口" && !m_presenter->motorController()->isMotorSerialOpen()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("提示"));
            msgBox.setText(QStringLiteral("电机串口未打开，请在设置中配置"));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
            msgBox.exec();
            return false;
        }
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        if (!m_presenter->motorController()->isMotorTcpOpen()) {
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

    if (isModbus && m_presenter->motorController()->isMotorSerialOpen()) {
        m_presenter->motorController()->motorCheckMode();
    }
}

//============================================================================
// on_comboWorkMode_currentIndexChanged - 工作模式下拉框切换
//   0 = 关闭AI, 1 = 目标识别, 2 = 自动跟踪, 3 = 点选跟踪, 4 = 框选跟踪
//============================================================================
void MainWindow::on_comboWorkMode_currentIndexChanged(int index)
{
    // 非点选/框选跟踪模式时禁止鼠标框选（本地 UI 状态，不涉及设备指令）
    if (auto vw = m_videoGrid->getWidget("default_device")) vw->setSelectionEnabled(index == 3 || index == 4);

    if (m_updatingFromDevice) return;

    if (!requireConnected()) {
        m_updatingFromDevice = true;
        ui->comboWorkMode->setCurrentIndex(m_previousWorkMode);
        m_updatingFromDevice = false;
        return;
    }
    m_presenter->motorController()->setWorkMode(index);
    m_previousWorkMode = index;
    m_presenter->motorController()->queryImageParams();
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
    sendAlgoModel(index * 10 + (low >= 0 ? low + 2 : 0));
}
void MainWindow::on_comboAlgoModel2_currentIndexChanged(int index)
{
    int high = ui->comboAlgoModel1->currentIndex();
    sendAlgoModel(high * 10 + (index + 2));
}
void MainWindow::sendAlgoModel(int model)
{
    if (m_updatingFromDevice) return;
    if (!requireConnected()) { return; }
    m_currentAlgoModel = model;
    m_presenter->motorController()->setAlgoModel(model);
    m_previousAlgoModel = model;
    m_presenter->motorController()->queryImageParams();
}

//============================================================================
// on_comboDisplayMode_currentIndexChanged - 显示模式下拉框切换
// 切换时下发显示模式变更指令，镜头目标由显示模式自动判断
//============================================================================
void MainWindow::on_comboDisplayMode_currentIndexChanged(int index)
{
    if (!requireConnected()) { ui->comboDisplayMode->blockSignals(true); ui->comboDisplayMode->setCurrentIndex(m_previousDisplayMode); ui->comboDisplayMode->blockSignals(false); return; }
    if (m_updatingFromDevice) return;
    // 根据显示模式自动切换算法模型：0/2/3→可见光模型，1/4→红外模型
    // 直接下发不触发 queryImageParams，避免设备返回旧数据覆盖显示模式
    {
        int algoIdx = (index == 1 || index == 4) ? 1 : 0;
        if ((m_currentAlgoModel / 10) != algoIdx) {
            int low = ui->comboAlgoModel2->currentIndex();
            int model = algoIdx * 10 + (low >= 0 ? low + 2 : 0);
            m_currentAlgoModel = model;
            m_presenter->motorController()->setAlgoModel(model);
            ui->comboAlgoModel1->blockSignals(true);
            ui->comboAlgoModel1->setCurrentIndex(algoIdx);
            ui->comboAlgoModel1->blockSignals(false);
        }
    }
    // 延后发送显示模式，避免与 setAlgoModel 间隔过近被设备忽略
    QTimer::singleShot(150, this, [this]() {
        if (m_presenter->tcpClient()->isConnected()) {
            int idx = ui->comboDisplayMode->currentIndex();
            m_presenter->motorController()->setDisplayMode(idx);
        }
    });
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



int MainWindow::currentAlgoModel() const
{
    return m_currentAlgoModel;
}


void MainWindow::refreshStyle(QWidget *w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

// ============================================================================
// 此文件包含 MainWindow 中庞大的 JSON 解析与 UI 更新逻辑。
// 作为向 MVP 架构过渡的中间步骤，这部分代码从 mainwindow.cpp 中剥离，
// 未来将进一步下沉至 DeviceContext 与 JsonFrameParser 中。
// ============================================================================

//============================================================================
// updateStatusFromJson - JSON 帧解析与 UI 状态更新（核心方法）
// 根据 ControlType 字段分发处理三种数据类型：
//   AIInfo     → 识别/跟踪结果 (Object 列表、脱靶量、锁定状态等)
//   ZoomInfo   → 镜头变倍信息、GPS 坐标、云台角度、激光测距
//   ImageSetting → 图像参数 (分辨率/码率/编码/工作模式/显示模式/算法模型)
//============================================================================
void MainWindow::updateStatusFromJson(const QJsonObject& doc)
{
    QString controlType = doc.value("ControlType").toString();
    CameraConfig& cam = m_cfg->cam();

    //==========================================================================
    // 1) AIInfo - AI 识别与跟踪结果帧
    //==========================================================================
    if (controlType == "AIInfo") {
        m_lastAiInfoTime = QDateTime::currentDateTime();
        int workMode = doc.value("WorkMode").toInt();
        int count = doc.value("ObjectCount").toInt();

        if (workMode == 1) {
            //==================================================================
            // 识别模式 (WorkMode=1)：
            // 遍历 Object 字典，将每个目标的 ID/类别/距离/像素位置/脱靶量
            // 填入识别结果表格 tableIdentify
            //==================================================================
            ui->lblIdentifyCount->setText(QString::fromUtf8("目标总数: %1").arg(count));
            ui->tableIdentify->setRowCount(0);  // 清空旧数据，重新填充

            // 根据当前显示模式判断使用可见光还是红外参数
            // combo 索引: 0=大图可见光, 1=红外, 2=可见光, 3=融合, 4=大图红外
            bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
            double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                              : cam.irMinFocal * m_currentIrZoom;
            int halfW = (isVis ? m_currentResX : cam.irResX) / 2;
            int halfH = (isVis ? m_currentResY : cam.irResY) / 2;

            // Object 字段是一个字典，key 为目标 ID，value 为目标属性
            if (doc.contains("Object") && doc.value("Object").isObject()) {
                QJsonObject objMap = doc.value("Object").toObject();
                for (auto it = objMap.begin(); it != objMap.end(); ++it) {
                    QString id = it.key();
                    QJsonObject obj = it.value().toObject();

                    int cls = obj.value("Class").toInt();
                    double dist = calcVisualDistance(obj, cls, false);
                    if (dist > 0) {
                        m_lastAiDist = dist;
                        m_lastAiDistEstimated = (obj.value("Distance").toDouble(0) <= 0);
                    }

                    int r = ui->tableIdentify->rowCount();
                    ui->tableIdentify->insertRow(r);
                    ui->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
                    ui->tableIdentify->setItem(r, 1, new QTableWidgetItem(QString::number(cls)));
                    ui->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(dist, 'f', 1)));

                    if (obj.contains("Points")) {
                        QJsonObject pts = obj.value("Points").toObject();
                        int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                        int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                        QString pos = QString("(%1,%2)").arg(l).arg(t);
                        ui->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));

                        // 计算目标中心相对于画面中心的脱靶量（毫弧度）
                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        QString miss = GeoCalculator::missMradStr(cx - halfW, cy - halfH, px, fl);
                        ui->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
                    }
                }
            }
        }

        // 识别模式与跟踪模式都需要更新地图上的目标标记
        if ((workMode == 1) || (workMode >= 2 && workMode <= 4))
            updateMapTargets(doc, workMode);

        //==================================================================
        // 跟踪模式 (WorkMode=2~4)：
        //   2 = 自动跟踪, 3 = 点选跟踪, 4 = 波门/框选跟踪
        // 显示锁定状态、目标 ID、类别、距离、角度、像素框、脱靶量
        // Class=0xB1 表示锁定，否则为丢失
        //==================================================================
        if (workMode >= 2 && workMode <= 4) {
            bool hasObj = doc.contains("Object") && doc.value("Object").isObject()
                          && !doc.value("Object").toObject().isEmpty();

            if (hasObj) {
                QJsonObject objMap = doc.value("Object").toObject();
                QJsonObject obj = objMap.begin().value().toObject();
                int cls = obj.value("Class").toInt();

                bool locked = (cls == 0xB1);
                QString statusText = locked ? QString::fromUtf8("锁定中") : QString::fromUtf8("丢失");
                QString statusFull = QString::fromUtf8("状态: %1").arg(statusText);
                ui->lblTrackStatus->setText(statusFull);
                ui->lblTrackStatus->setProperty("state", locked ? "locked" : "missed");
                refreshStyle(ui->lblTrackStatus);

                if (obj.contains("Distance")) {
                    double rawDist = obj.value("Distance").toDouble(0);
                    if (rawDist > 0)
                        ui->trackDistance->setText(QString::number(rawDist, 'f', 1) + QStringLiteral(" m"));
                    // rawDist==0: 保留 calcVisualDistance 设置的估算值
                } else
                    ui->trackDistance->clear();

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    int cx = (l + r2) / 2, cy = (t + b) / 2;
                    int pw = r2 - l, ph = b - t;
                    ui->trackPos->setText(QString("(%1,%2) %3×%4").arg(cx).arg(cy).arg(pw).arg(ph));

                    // 计算脱靶量：像素偏移 × 像元尺寸 / 焦距 → 毫弧度
                    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
                    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
                    double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                                      : cam.irMinFocal * m_currentIrZoom;
                    int halfW = (isVis ? m_currentResX : cam.irResX) / 2;
                    int halfH = (isVis ? m_currentResY : cam.irResY) / 2;
                    double objCx = (l + r2) / 2.0, objCy = (t + b) / 2.0;
                    double dx = objCx - halfW, dy = objCy - halfH;
                    double dxMrad = dx * px / fl;
                    double dyMrad = dy * px / fl;
                    ui->trackMissDistance->setText(QString("H: %1  V: %2 mrad")
                        .arg(dxMrad, 0, 'f', 2).arg(dyMrad, 0, 'f', 2));
                } else {
                    ui->trackPos->clear();
                    ui->trackMissDistance->clear();
                }
            } else {
                // 无目标：显示"未锁定"并清空所有跟踪字段
                ui->lblTrackStatus->setText(QString::fromUtf8("状态: 未锁定"));
                ui->lblTrackStatus->setProperty("state", "nolock");
                refreshStyle(ui->lblTrackStatus);
                ui->trackPos->clear();
                ui->trackMissDistance->clear();
                ui->trackDistance->clear();
            }
        }

    //==========================================================================
    // 2) ZoomInfo - 镜头倍率与设备状态帧
    // 更新变倍倍率、GPS 坐标、高度、激光测距、云台水平/垂直角
    // 同时触发镜头统计信息更新与地图设备位置更新
    //==========================================================================
    } else if (controlType == "ZoomInfo") {
        m_currentVisZoom = doc.value("ZoomInfo").toDouble(1.0);
        m_currentIrZoom = doc.value("ZoomInfoIR").toDouble(1.0);

        ui->statCamMode->setText(QString::number(doc.value("CamShowMode").toInt()));
        ui->statLatitude->setText(doc.value("Latitude").toString());
        ui->statLongitude->setText(doc.value("Longitude").toString());
        {
            double h = doc.value("Height").toDouble();
            if (h != 0.0)
                ui->statHeight->setText(QString::number(h, 'f', 1) + QStringLiteral(" m"));
            else
                ui->statHeight->clear();
        }

        double rawPan = doc.value("PTZInfoH").toDouble();
        double rawTilt = doc.value("PTZInfoV").toDouble();

        if (m_cfg->softwarePtzCalibrationEnabled()) {
            rawPan -= m_cfg->ptzPanOffset();
            while (rawPan < 0) rawPan += 360.0;
            while (rawPan >= 360.0) rawPan -= 360.0;

            rawTilt -= m_cfg->ptzTiltOffset();
            while (rawTilt < -180.0) rawTilt += 360.0;
            while (rawTilt > 180.0) rawTilt -= 360.0;
        }

        ui->statPanAngle->setText(QString::number(rawPan, 'f', 1) + QStringLiteral("°"));
        m_currentTilt = rawTilt;
        ui->statTiltAngle->setText(QString::number(rawTilt, 'f', 1) + QStringLiteral("°"));

        updateLensStats();
        updateMapDevicePosition(doc);

    //==========================================================================
    // 3) ImageSetting - 图像参数配置帧
    // 设备主动推送或响应查询，更新分辨率/码率/编码/工作模式/显示模式/算法
    // 并根据设备当前值同步 UI 下拉框，同时设置 m_updatingFromDevice 标志
    // 防止 UI 变化再次触发设备指令造成死循环
    //==========================================================================
    } else if (controlType == "ImageSetting") {
        // 图像分辨率映射表
        static const char* resMap[] = {"1080P", "720P", "D1", "1440P"};
        int imgSize = doc.value("ImageSize").toInt();
        ui->paramResolution->setText(imgSize >= 0 && imgSize < 4 ? resMap[imgSize] : QString::number(imgSize));
        {
            static const int resTab[][2] = {{1920,1080},{1280,720},{704,576},{2566,1520}};
            if (imgSize >= 0 && imgSize < 4) {
                m_currentResX = resTab[imgSize][0];
                m_currentResY = resTab[imgSize][1];
            }
        }

        // 图像码率
        ui->paramBitrate->setText(QString("%1 Kb/s").arg(doc.value("ImageBit").toInt()));

        // 编码格式映射表
        static const char* codecMap[] = {"H264", "H265"};
        int codec = doc.value("ImageCode").toInt();
        ui->paramCodec->setText(codec >= 0 && codec < 2 ? codecMap[codec] : QString::number(codec));

        // 工作模式映射表
        static const char* wmMap[] = {"关闭AI", "识别", "自动跟踪", "点选跟踪", "波门/框选跟踪"};
        int wm = doc.value("WorkMode").toInt();
        ui->paramWorkMode->setText(wm >= 0 && wm < 5 ? QString::fromUtf8(wmMap[wm]) : QString::number(wm));
        m_previousWorkMode = wm;

        // 显示类型映射表 (PIP = Picture-in-Picture)
        static const char* pipMap[] = {"大图可见光", "红外", "可见光", "融合", "大图红外"};
        int pipRaw = doc.value("PipShow").toInt();
        int comboIdx = DeviceController::pipShowToComboIndex(pipRaw);
        ui->paramPipShow->setText(comboIdx >= 0 && comboIdx < 5 ? QString::fromUtf8(pipMap[comboIdx]) : QString::number(pipRaw));

        // 算法模型编码: 高段(传感器)×10 + 低段(识别类型)
        int model = doc.value("Model").toInt();
        int high = model / 10;
        int low  = model % 10;
        static const char* highMap[] = {"可见光", "红外"};
        static const char* lowMap[]  = {"", "", "人车识别", "船识别", "无人机识别", "飞机直升机识别", "鸟识别"};
        QString modelStr;
        if (high >= 0 && high < 2)
            modelStr = QString::fromUtf8(highMap[high]);
        if (low >= 2 && low <= 6)
            modelStr += QString(" / %1").arg(QString::fromUtf8(lowMap[low]));
        ui->paramAlgoModel->setText(modelStr.isEmpty() ? QString::number(model) : modelStr);
        m_previousAlgoModel = model;

        ui->paramMaxVisFL->setText(doc.value("MaxVisFL").toString());
        ui->paramMaxIRFL->setText(doc.value("MaxIRFL").toString());

        m_currentPipShow = DeviceController::pipShowToComboIndex(doc.value("PipShow").toInt());
        m_previousDisplayMode = m_currentPipShow;

        // 同步 UI 下拉框到设备当前值，同时抑制信号递归
        m_updatingFromDevice = true;
        // 首次连接时同步算法模型下拉框，后续不再覆盖用户选择
        if (!m_algoModelInitialized) {
            m_currentAlgoModel = model;
            // 高段 = 传感器类型 (0=可见光, 1=红外) → comboAlgoModel1
            if (high >= 0 && high < ui->comboAlgoModel1->count())
                ui->comboAlgoModel1->setCurrentIndex(high);
            // 低段 = 识别类型 (2-6 → comboAlgoModel2 索引 0-4)
            if (low >= 2 && low <= 6)
                ui->comboAlgoModel2->setCurrentIndex(low - 2);
            m_algoModelInitialized = true;
        }
        int pipShow = doc.value("PipShow").toInt();
        if (!m_displayModeInitialized) {
            int comboIdx = DeviceController::pipShowToComboIndex(pipShow);
            if (comboIdx >= 0 && comboIdx < ui->comboDisplayMode->count()) {
                ui->comboDisplayMode->setCurrentIndex(comboIdx);
                m_displayModeInitialized = true;
            }
        }
        // 首次连接时同步工作模式下拉框，后续不再覆盖用户选择
        if (!m_workModeInitialized && wm >= 0 && wm < ui->comboWorkMode->count()) {
            ui->comboWorkMode->setCurrentIndex(wm);
            m_workModeInitialized = true;
        }
        m_updatingFromDevice = false;
    }
}

//============================================================================
// updateMapDevicePosition - 更新地图上的设备位置与视场角
// 从 ZoomInfo JSON 帧中解析 GPS、云台角度、激光测距等数据，
// 计算当前镜头的水平/垂直视场角，绘制到地图控件上
//
// 视场角计算：
//   HFOV = 2 × arctan(传感器宽度_mm / (2 × 焦距_mm))
//   VFOV = HFOV × 9/16 (假定 16:9 传感器宽高比)
// 传感器宽度 = 像元尺寸 × 水平分辨率 / 1000
//============================================================================
void MainWindow::updateMapDevicePosition(const QJsonObject& doc)
{
    QString latStr = doc.value("Latitude").toString();
    QString lonStr = doc.value("Longitude").toString();
    double lat = GeoCalculator::parseCoord(latStr);
    double lon = GeoCalculator::parseCoord(lonStr);
    double alt = doc.value("Height").toDouble(0);
    double pan = doc.value("PTZInfoH").toDouble(0);
    double tilt = doc.value("PTZInfoV").toDouble(0);

    if (m_cfg->softwarePtzCalibrationEnabled()) {
        pan -= m_cfg->ptzPanOffset();
        while (pan < 0) pan += 360.0;
        while (pan >= 360.0) pan -= 360.0;

        tilt -= m_cfg->ptzTiltOffset();
        while (tilt < -180.0) tilt += 360.0;
        while (tilt > 180.0) tilt -= 360.0;
    }
    double range = doc.value("LaserRange").toDouble(0);
    bool rangeEstimated = false;
    if (range <= 0) {
        range = m_lastAiDist;
        rangeEstimated = m_lastAiDistEstimated;
    }

    qDebug() << "[MapPos] raw:" << latStr << lonStr << "parsed:" << lat << lon;
    if (lat == 0 && lon == 0) return;

    m_mapWidget->setDevicePosition(lat, lon);

    // 计算可见光视场角
    CameraConfig& cam = m_cfg->cam();
    double visSensorW = cam.visPixelSize * cam.visResX / 1000.0;
    double visFocal = cam.visMinFocal * m_currentVisZoom;
    double visHfov = 2.0 * qAtan(visSensorW / (2.0 * visFocal)) * 180.0 / M_PI;
    double visVfov = visHfov * cam.visResY / cam.visResX;

    // 计算红外视场角
    double irSensorW = cam.irPixelSize * cam.irResX / 1000.0;
    double irFocal = cam.irMinFocal * m_currentIrZoom;
    double irHfov = 2.0 * qAtan(irSensorW / (2.0 * irFocal)) * 180.0 / M_PI;
    double irVfov = irHfov * cam.irResY / cam.irResX;

    // 可见光视场角 4km（蓝色），红外视场角 2km（红色）
    m_mapWidget->setVisFov(lat, lon, pan, tilt, visHfov, visVfov, 4000);
    m_mapWidget->setIrFov(lat, lon, pan, tilt, irHfov, irVfov, 2000);
    m_mapWidget->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, rangeEstimated);
}

//============================================================================
// updateMapTargets - 更新地图上的 AI 目标标记
// 跟踪模式 (WorkMode 2~4)：
//   - 仅显示 1 个目标（锁定 0xB1 优先，丢失 0xB2 次之）
//   - 首次失锁（0xB2）时记录时间，保持最后位置 5 秒
//   - 失锁超 5 秒清除轨迹和目标点
// 识别模式 (WorkMode 1)：显示全部识别目标，无上报时清空遗留
//============================================================================
void MainWindow::updateMapTargets(const QJsonObject& doc, int workMode)
{
    CameraIntrinsics camInfo;
    CameraConfig& camCfg = m_cfg->cam();
    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
    camInfo.pixelSizeUm = isVis ? camCfg.visPixelSize : camCfg.irPixelSize;
    camInfo.focalLengthMm = isVis ? camCfg.visMinFocal * m_currentVisZoom : camCfg.irMinFocal * m_currentIrZoom;
    camInfo.resX = isVis ? camCfg.visResX : camCfg.irResX;
    camInfo.resY = isVis ? camCfg.visResY : camCfg.irResY;

    DevicePose devPose;
    devPose.lat = GeoCalculator::parseCoord(ui->statLatitude->text());
    devPose.lon = GeoCalculator::parseCoord(ui->statLongitude->text());
    devPose.panDeg = ui->statPanAngle->text().toDouble();

    bool hasObject = doc.contains("Object") && doc.value("Object").isObject();
    QJsonObject objMap;
    if (hasObject) objMap = doc.value("Object").toObject();
    double tilt = m_currentTilt;

    //==========================================================================
    // 识别模式 (WorkMode=1)：显示所有目标，无上报时清空
    //==========================================================================
    if (workMode == 1) {
        if (!hasObject || objMap.isEmpty()) {
            m_mapWidget->clearAllTracks();
            m_mapWidget->clearFov();
            m_mapWidget->updateTargetMarkers(QJsonArray());
            return;
        }

        QJsonArray targetArr;
        for (auto it = objMap.begin(); it != objMap.end(); ++it) {
            QString id = it.key();
            QJsonObject obj = it.value().toObject();
            int cls = obj.value("Class").toInt();
            double tLat = 0, tLon = 0;

            double dist = calcVisualDistance(obj, cls, false);

            if (obj.contains("Points")) {
                QJsonObject pts = obj.value("Points").toObject();
                int L = pts.value("Left").toInt(), T = pts.value("Top").toInt();
                int R = pts.value("Right").toInt(), B = pts.value("Bottom").toInt();
                double cx = (L + R) / 2.0, cy = (T + B) / 2.0;
                GeoCalculator::pixelToGps(cx, cy, dist, camInfo, devPose, tLat, tLon);

                QJsonArray bbox;
                double bLat, bLon;
                GeoCalculator::pixelBboxToGps(L, T, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoCalculator::pixelBboxToGps(R, T, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoCalculator::pixelBboxToGps(R, B, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoCalculator::pixelBboxToGps(L, B, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});

                QJsonObject t;
                t[QStringLiteral("id")] = id;
                t[QStringLiteral("cls")] = cls;
                t[QStringLiteral("dist")] = dist;
                t[QStringLiteral("lat")] = tLat;
                t[QStringLiteral("lon")] = tLon;
                t[QStringLiteral("locked")] = false;
                t[QStringLiteral("bbox")] = bbox;
                targetArr.append(t);
            }
        }
        m_mapWidget->updateTargetMarkers(targetArr);
        return;
    }

    //==========================================================================
    // 跟踪模式 (WorkMode=2~4)
    //==========================================================================
    if (workMode >= 2 && workMode <= 4) {

        // -- 查找锁定目标 0xB1 和丢失目标 0xB2 --
        QString lockedId, lostId;
        QJsonObject lockedObj, lostObj;
        for (auto it = objMap.begin(); it != objMap.end(); ++it) {
            QJsonObject obj = it.value().toObject();
            int cls = obj.value("Class").toInt();
            if (cls == 0xB1 && lockedId.isEmpty()) {
                lockedId = it.key(); lockedObj = obj;
            } else if (cls == 0xB2 && lostId.isEmpty()) {
                lostId = it.key(); lostObj = obj;
            }
        }

        // ---- 有锁定目标 ----
        if (!lockedId.isEmpty()) {
            m_track.lostSince = QDateTime();

            int cls = lockedObj.value("Class").toInt();
            double tLat = 0, tLon = 0;

            double dist = calcVisualDistance(lockedObj, cls, true);
            // 缓存 AI 目标距离（用于 ZoomInfo 无激光测距时回退）
            m_lastAiDist = dist;
            m_lastAiDistEstimated = (lockedObj.value("Distance").toDouble(0) <= 0 && dist > 0);

            if (lockedObj.contains("Points")) {
                QJsonObject pts = lockedObj.value("Points").toObject();
                int L = pts.value("Left").toInt(), T = pts.value("Top").toInt();
                int R = pts.value("Right").toInt(), B = pts.value("Bottom").toInt();
                double cx = (L + R) / 2.0, cy = (T + B) / 2.0;
                GeoCalculator::pixelToGps(cx, cy, dist, camInfo, devPose, tLat, tLon);

                m_track.lat = tLat;
                m_track.lon = tLon;
                m_track.cls = cls;

                // 计算速度（米/秒）
                double speed = 0;
                if (m_track.prevTime.isValid()) {
                    double dist_m = GeoCalculator::haversineDistance(m_track.prevLat, m_track.prevLon, tLat, tLon);
                    double dt_s = m_track.prevTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
                    if (dt_s > 0) speed = dist_m / dt_s;
                }
                m_track.prevLat = tLat;
                m_track.prevLon = tLon;
                m_track.prevTime = QDateTime::currentDateTime();

                // 轨迹点抽稀判定
                if (tLat != 0 && tLon != 0) {
                    // 目标切换时重置抽稀状态，确保新目标首点必定绘制
                    bool targetChanged = (m_track.id != lockedId);
                    m_track.id = lockedId;
                    if (targetChanged)
                        m_track.plotHeading = -1;

                    double outBearing = 0;
                    if (GeoCalculator::shouldPlotTrackPoint(tLat, tLon,
                                             m_track.plotLat, m_track.plotLon,
                                             m_track.plotHeading, m_track.plotTime,
                                             &outBearing)) {
                        m_mapWidget->appendTrackPoint(lockedId, tLat, tLon, speed);
                        m_track.plotLat = tLat;
                        m_track.plotLon = tLon;
                        m_track.plotTime = QDateTime::currentDateTime();
                        m_track.plotHeading = outBearing;
                    }
                }

                QJsonArray targetArr;
                QJsonObject t;
                t[QStringLiteral("id")] = lockedId;
                t[QStringLiteral("cls")] = cls;
                t[QStringLiteral("dist")] = dist;
                t[QStringLiteral("lat")] = tLat;
                t[QStringLiteral("lon")] = tLon;
                t[QStringLiteral("locked")] = true;
                t[QStringLiteral("speed")] = speed;

                QJsonArray bbox;
                double bLat, bLon;
                GeoCalculator::pixelBboxToGps(L, T, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoCalculator::pixelBboxToGps(R, T, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoCalculator::pixelBboxToGps(R, B, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                GeoCalculator::pixelBboxToGps(L, B, dist, tilt, camInfo, devPose, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                t[QStringLiteral("bbox")] = bbox;
                targetArr.append(t);
                m_mapWidget->updateTargetMarkers(targetArr);
            }
            return;
        }

        // ---- 有丢失目标 (0xB2) ----
        if (!lostId.isEmpty()) {
            int cls = lostObj.value("Class").toInt();
            double tLat = 0, tLon = 0;
            bool hasPts = false;

            double dist = calcVisualDistance(lostObj, cls, true);

            if (lostObj.contains("Points")) {
                QJsonObject pts = lostObj.value("Points").toObject();
                int L = pts.value("Left").toInt(), T = pts.value("Top").toInt();
                int R = pts.value("Right").toInt(), B = pts.value("Bottom").toInt();
                double cx = (L + R) / 2.0, cy = (T + B) / 2.0;
                GeoCalculator::pixelToGps(cx, cy, dist, camInfo, devPose, tLat, tLon);
                hasPts = true;
            }

            // 首次失锁：保存位置，记录时间
            if (m_track.lostSince.isNull()) {
                m_track.id = lostId;
                m_track.lat = tLat;
                m_track.lon = tLon;
                m_track.cls = cls;
                m_track.lostSince = QDateTime::currentDateTime();
            }

            // 检查是否超过 5 秒
            qint64 elapsed = m_track.lostSince.msecsTo(QDateTime::currentDateTime());
            if (elapsed >= 5000) {
                // 超过 5 秒，清除轨迹和目标
                m_mapWidget->clearAllTracks();
                m_mapWidget->updateTargetMarkers(QJsonArray());
                return;
            }

            // 5 秒内：显示最后位置
            if (hasPts) {
                QJsonArray targetArr;
                QJsonObject t;
                t[QStringLiteral("id")] = m_track.id;
                t[QStringLiteral("cls")] = m_track.cls;
                t[QStringLiteral("dist")] = dist;
                t[QStringLiteral("lat")] = m_track.lat;
                t[QStringLiteral("lon")] = m_track.lon;
                t[QStringLiteral("locked")] = false;
                t[QStringLiteral("speed")] = 0;
                targetArr.append(t);
                m_mapWidget->updateTargetMarkers(targetArr);
            }
            return;
        }

        // ---- 有 Object 但无 0xB1/0xB2，清空 ----
        m_mapWidget->clearAllTracks();
        m_mapWidget->updateTargetMarkers(QJsonArray());
    }
}

//============================================================================
// updateLensStats - 更新镜头统计数据
// 根据当前变倍倍率计算可见光与红外的：
//   - 当前焦距 (最小焦距 × 倍率)
//   - 水平视场角 (HFOV): 2 × arctan(传感器宽度 / (2 × 焦距))
// 传感器宽度 = 像元尺寸 × 水平分辨率 (单位换算为 mm)
//============================================================================
void MainWindow::updateLensStats()
{
    CameraConfig& cam = m_cfg->cam();
    const double kRad2Deg = 180.0 / 3.14159265358979323846;

    double visFocal = cam.visMinFocal * m_currentVisZoom;
    double irFocal  = cam.irMinFocal * m_currentIrZoom;

    ui->statZoomVis->setText(QString::number(m_currentVisZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalVis->setText(QString::number(visFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusVis->clear();

    // HFOV = 2 * atan( sensor_width_mm / (2 * focal_mm) )
    double visHfov = 2.0 * qAtan((cam.visPixelSize * cam.visResX / 1000.0) / (2.0 * visFocal));
    ui->statFovVis->setText(QString::number(visHfov * kRad2Deg, 'f', 2) + QStringLiteral("°"));

    ui->statZoomIR->setText(QString::number(m_currentIrZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusIR->clear();

    double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));
    ui->statFovIR->setText(QString::number(irHfov * kRad2Deg, 'f', 2) + QStringLiteral("°"));
}

// calcVisualDistance - 封装了"无激光测距时用视觉法估算距离"的公共逻辑
// obj: 目标 JSON 对象（已有 Distance 字段和 Points 字段）
// cls: 目标 Class 编码
// updateTrackLabel: 是否更新 trackDistance 状态栏文本（跟踪锁定/丢失时 true）
// 返回值：已有激光距离则返回原值，否则返回估算值
double MainWindow::calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel)
{
    double dist = obj.value("Distance").toDouble(0);
    if (dist > 0 || !obj.contains("Points"))
        return dist;

    int low = currentAlgoModel() % 10;
    double ref = m_cfg->cam().targetRefSize(low, cls);

    // 跟踪状态 (0xB1/0xB2) 无法通过 Class 查到参考尺寸
    // → 用算法模型遍历已知 Class 做视觉估算
    if (ref <= 0 && (cls == 0xB1 || cls == 0xB2)) {
        static const int fallback[] = {0xA1, 0xA2, 0xA3, 0xA4};
        for (int fc : fallback) {
            ref = m_cfg->cam().targetRefSize(low, fc);
            if (ref > 0) break;
        }
    }

    if (ref <= 0)
        return dist;

    QJsonObject pts = obj.value("Points").toObject();
    int boxPx = qMax(pts.value("Right").toInt() - pts.value("Left").toInt(),
                     pts.value("Bottom").toInt() - pts.value("Top").toInt());
    if (boxPx <= 0)
        return dist;

    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
    double pxSize = isVis ? m_cfg->cam().visPixelSize : m_cfg->cam().irPixelSize;
    double focal = isVis ? m_cfg->cam().visMinFocal * m_currentVisZoom
                          : m_cfg->cam().irMinFocal * m_currentIrZoom;

    dist = GeoCalculator::estimateTargetDistance(boxPx, focal, pxSize, ref);
    if (updateTrackLabel)
        ui->trackDistance->setText(QString::number(dist, 'f', 1) + QStringLiteral(" m (估算)"));
    return dist;
}

