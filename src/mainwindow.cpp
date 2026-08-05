//============================================================================
// mainwindow.cpp - T-JSON ������ʵ��
// ���������ڵĹ���/������UI ��ʽ��ʼ�����ź�-�����ӣ�
// �Լ������� JSON ֡������״̬���¡���ͼ����ת������̨��ͷ�����߼���
//============================================================================
#include "mainwindow.h"
#include "core/GeoCalculator.h"
#include "ui_mainwindow.h"
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

static double parseCoord(const QString& s);
static double haversineDistance(double lat1, double lon1, double lat2, double lon2);
static double bearing(double lat1, double lon1, double lat2, double lon2);

//============================================================================
// ����������ˢ�¿ؼ��� QSS ��̬����
//============================================================================
static void refreshStyle(QWidget *w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

//============================================================================
// ���캯������ʼ��������ģ�顢�����ź�-�����ӡ����� UI
//============================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_cfg(new ConfigManager(this))
    , m_presenter(new MainPresenter(this, m_cfg, this))           // RTSP ��Ƶ�����߳�
    , m_updatingFromDevice(false)            // ���ݹ���³�ʼ�ر�
    , m_currentVisZoom(1.0)                  // Ĭ�Ͽɼ��ⱶ�� 1.0
    , m_currentIrZoom(1.0)                   // Ĭ�Ϻ��ⱶ�� 1.0
    , m_currentTilt(0.0)                     // Ĭ�ϸ����� 0
    , m_currentPipShow(0)                    // Ĭ����ʾģʽ����ͼ�ɼ���
    , m_workModeInitialized(false)
    , m_currentResX(m_cfg->cam().visResX)    // Ĭ�Ͽɼ���ֱ���
    , m_currentResY(m_cfg->cam().visResY)
{
    ui->setupUi(this);

    setWindowIcon(QIcon(QStringLiteral(":/qss/logo.ico")));

    setupUiStyles();

    ui->titleBar->installEventFilter(this);
    ui->titleBar->setProperty("form", "title");
    ui->labelAppIcon->setPixmap(QPixmap(QStringLiteral(":/qss/logo.png")).scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->btnMenu_Min->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/minimize.png")));
    ui->btnMenu_Max->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/maximize.png")));
    ui->btnMenu_Close->setIcon(QIcon(QStringLiteral(":/qss/blacksoft/close.png")));
    for (auto *b : {ui->btnMenu_Min, ui->btnMenu_Max, ui->btnMenu_Close})
        b->setIconSize(QSize(18, 18));

    // Ϊ��������ť���� SVG ͼ�꣨ͼƬ���ϣ��������£�
    auto setupNavBtn = [](QToolButton* btn, const QString& svgPath) {
        btn->setIcon(QIcon(svgPath));
        btn->setIconSize(QSize(18, 18));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    };
    setupNavBtn(ui->btnNavMonitor, QStringLiteral(":/monitor.svg"));
    setupNavBtn(ui->btnNavPlayback, QStringLiteral(":/playback.svg"));
    setupNavBtn(ui->btnNavLog, QStringLiteral(":/log.svg"));
    setupNavBtn(ui->btnNavSettings, QStringLiteral(":/gear.svg"));

    // ������ť������
    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    navGroup->addButton(ui->btnNavMonitor, 0);
    navGroup->addButton(ui->btnNavPlayback, 1);
    navGroup->addButton(ui->btnNavLog, 2);
    navGroup->addButton(ui->btnNavSettings, 3);
    ui->btnNavMonitor->setChecked(true);

    // ���������Զ���ʼ������
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        m_presenter->motorController()->openMotorSerial(m_cfg->motorComPort());
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_presenter->motorController()->openMotorTcp();
    }

    connect(m_presenter->ptzForwarder(), &PtzForwarder::ptzAnglesUpdated, this, [this](double, double) {
        // ת̨�Ƕ��Ѹ��� 8089 �˿� JSON ���ݸ���
    });

    // PTZ Forwarder start���ӳٵ��¼�ѭ�������
    QTimer::singleShot(0, this, [this]() {
        if (m_cfg->serialServerEnabled()) {
            m_presenter->ptzForwarder()->start(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
            m_presenter->ptzForwarder()->setOffsets(m_cfg->ptzPanOffset(), m_cfg->ptzTiltOffset());
        }
    });


    // �����ͼ������ �� MapWidget �� ���ǲ�
    m_mapContainer = new QWidget(ui->widgetDisplay);
    m_mapContainer->setVisible(false);
    m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);
    m_mapWidget = new MapWidget(m_mapContainer);
    m_mapWidget->setGeometry(0, 0, 280, 280);
    // ͸�����ǲ㣺����ģʽ������꣨��ק�ƶ���˫��չ����
    m_mapOverlay = new QWidget(m_mapContainer);
    m_mapOverlay->setGeometry(0, 0, 280, 280);
    m_mapOverlay->setCursor(Qt::OpenHandCursor);
    m_mapOverlay->installEventFilter(this);

    // PiP �����Ի��򣺴��ͼʱ��Ƶ��ʾ�ڴ�
    // ���� 2566:1520 ��߱ȣ��ö���ʾ���߶�����ϱ�����22px
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
    auto *btnClose = new QPushButton(QStringLiteral("?"), m_pipTitle);
    btnClose->setFixedSize(20, 20);
    btnClose->setCursor(Qt::ArrowCursor);
    btnClose->setStyleSheet(QStringLiteral(
        "QPushButton{background:transparent;color:#a0a0b0;border:none;font-size:13px;}"
        "QPushButton:hover{background:#e74c3c;color:#fff;border-radius:2px;}"
    ));
    connect(btnClose, &QPushButton::clicked, this, [this]() { m_pipDialog->hide(); });
    pipLay->addWidget(m_pipTitle);

    // MapWidget �������ź� �� MainWindow
    connect(m_mapWidget, &MapWidget::miniRequested,
            this, [this]() { toggleMapMode(); });
    connect(m_mapWidget, &MapWidget::closeRequested,
            this, [this]() { toggleMap(); });
    connect(m_mapWidget, &MapWidget::enlargeRequested,
            this, [this]() { toggleMapMode(); });
    connect(ui->btnMapToggle, &QPushButton::clicked,
            this, [this]() { toggleMap(); });

    // �״β���
    updateMapLayout();

    // ϵͳ������ѯ��500ms ���ڲ�ѯ�豸 ImageSetting
    m_sysParamTimer = new QTimer(this);
    m_sysParamTimer->setInterval(500);
    connect(m_sysParamTimer, &QTimer::timeout, this, &MainWindow::onSysParamTimerTimeout);

    // AIInfo ��ʱ������豸��Ŀ��ʱ����֡��2 ���޸��������������
    m_aiCleanupTimer = new QTimer(this);
    m_aiCleanupTimer->setInterval(1000);
    connect(m_aiCleanupTimer, &QTimer::timeout, this, &MainWindow::onAiCleanupTimeout);
    m_aiCleanupTimer->start();

    // �ָ��ϴεĿ���״̬
    ui->checkDigitalZoom->setChecked(m_cfg->digitalZoomEnabled());
    ui->checkAutoZoom->setChecked(m_cfg->autoZoomEnabled());
    ui->checkCaptureUpload->setChecked(m_cfg->captureUploadEnabled());
    ui->checkPosReset->setChecked(m_cfg->posResetEnabled());

    // ��ʼ����ѡ����״̬
    int wm = ui->comboWorkMode->currentIndex();
    ui->videoWidget->setSelectionEnabled(wm == 3 || wm == 4);

    //============================================================================
    // RTSP ��Ƶ���ź�����
    // RtspThread �ڹ����߳����������룬ͨ���źŽ�֡���ݴ������߳�
    // VideoWidget �� selectionFinished �ź����ڿ�ѡ����
    //============================================================================
    connect(m_presenter->videoStream(), &RtspThread::frameReady, this, &MainWindow::onRtspFrame);
    connect(m_presenter->videoStream(), &RtspThread::streamOpened, this, &MainWindow::onRtspOpened);
    connect(m_presenter->videoStream(), &RtspThread::streamError, this, &MainWindow::onRtspError);
    connect(ui->videoWidget, &VideoWidget::selectionFinished, this, &MainWindow::onVideoSelection);

    //============================================================================
    // T-JSON Э���ź�����
    // TJsonClient ���� TCP �����ӡ��������JSON ֡�շ����Զ�����
    //============================================================================
    connect(m_presenter->tcpClient(), &TJsonClient::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(m_presenter->tcpClient(), &TJsonClient::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);
    connect(m_presenter->tcpClient(), &TJsonClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_presenter->tcpClient(), &TJsonClient::jsonReceived, this, &MainWindow::onJsonReceived);
    connect(m_presenter->tcpClient(), &TJsonClient::imageSnapped, this, &MainWindow::onImageSnapped);
    connect(m_presenter->tcpClient(), &TJsonClient::ackReceived, this, &MainWindow::onAckReceived);
    
    // �Զ������źţ�ÿ����������ʱ���°�ť�ı���״̬����ʾ
    connect(m_presenter->tcpClient(), &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        Q_UNUSED(maxRetries);
        ui->btnConnect->setText(QString::fromUtf8("������(����:%1)").arg(attempt));
        ui->btnConnect->setEnabled(false);
        ui->btnConnect->setProperty("state", "reconnecting");
        refreshStyle(ui->btnConnect);
        ui->btnCancelConnect->setVisible(true);
        ui->statusbar->showMessage(QString::fromUtf8("���粨�������ڽ��е� %1 ���Զ�̽������...").arg(attempt));
    });
    // ����ʧ�ܣ��ָ���ť��ʼ״̬
    connect(m_presenter->tcpClient(), &TJsonClient::reconnectFailed, this, [this]() {
        ui->btnConnect->setText(QString::fromUtf8("�����豸"));
        ui->btnConnect->setEnabled(true);
        ui->btnConnect->setProperty("state", QVariant());
        refreshStyle(ui->btnConnect);
        ui->btnCancelConnect->setVisible(false);
        ui->statusbar->showMessage(QString::fromUtf8("����ʧ�ܣ��ѷ�������"), 5000);
    });

    //============================================================================
    // ��̨�˷������ (���� Pelco-D Э��)
    // ���°�ť �� ���ͳ���ת��ָ��ͷŰ�ť �� ����ָֹͣ��
    // �˸���ť�ֱ��Ӧ Up/Down/Left/Right ���ĸ��Խ��߷���
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
    // ��̨�ٶȿ���
    // ��������ֵ�����˫��󶨣�ֵ�ı�ʱ���浽���ó־û�
    // ˮƽ�봹ֱ�ٶ�ʹ����ͬ����ֵ
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
    // ��ͷ���� (Zoom �䱶 / Focus ����)
    // ���°�ť �� �����䱶/�������ͷŰ�ť �� ֹͣ
    // op ֵ: 0=ZoomIn, 1=ZoomOut, 2=FocusIn, 3=FocusOut
    // ��ͷĿ�������ʾģʽ�Զ��жϣ�PipShow 1/4=����(target=1)������=�ɼ���(target=0)
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
    // ��ͷ�ٶȿ��� (�䱶�ٶ� / �����ٶ�)
    // ��������ֵ�����˫��󶨣�ֵ�ı�ʱ�Զ���������
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
    // Ԥ��λ���� (����/����/ɾ��)
    // ͨ�� spinPreset ѡ��Ԥ��λ��ţ����� DeviceController �е�Э���װ
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
    // ���ӹ��ܿ��� (���ֱ䱶 / �Զ��佹 / ץ���ϴ� / λ�ù���)
    // ÿ�� CheckBox ֱ����Ӧ���豸ָ��
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
        ui->statWiperStatus->setText(isManual ? "�ֶ�" : "�Զ�");
    });
    connect(m_presenter->motorController(), &DeviceController::motorSerialError, this, [this](const QString& msg) {
        ui->statWiperStatus->setText("����");
        qWarning() << "������ڴ���:" << msg;
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
        ui->btnWiperSilent->setText(isSilent ? "��ģʽ" : "����ģʽ");
        ui->statusbar->showMessage(isSilent ? "������л�Ϊ������ģʽ (StealthChop)" : "������л�Ϊ����ģʽ (SpreadCycle)", 3000);
    });
    connect(ui->editWiperCurrent, &QLineEdit::editingFinished, this, [this]() {
        int ma = ui->editWiperCurrent->text().toInt();
        m_presenter->motorController()->motorSetCurrent(ma);
        ui->statusbar->showMessage(QString("�����·����̻��������: %1 mA").arg(ma), 3000);
    });

    connect(ui->btnPtzReset, &QPushButton::clicked, this, [this]() {
        if (!requireConnected()) return;
        m_presenter->motorController()->callPreset(0);
    });

    //============================================================================
    // ָ����־����
    // ʵʱ��ʾ�����·����豸��ָ�����ݣ����������Э�����
    //============================================================================
    m_logDialog = new CmdLogDialog(this);
    connect(m_presenter->motorController(), &DeviceController::commandSent, m_logDialog, &CmdLogDialog::appendLog);

    //============================================================================
    // ϵͳ����
    // �رմ���ʱ��С�������̣��Ҽ��˵����˳�����
    //============================================================================
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/qss/logo.ico")));
    m_trayIcon->setToolTip(QStringLiteral("LSS��Ƶ����ͻ���"));

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(QStringLiteral("��ʾ������"), this, &MainWindow::onTrayShow);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(QStringLiteral("�˳�"), this, &MainWindow::onTrayExit);

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
// �����������ͷ� UI ��Դ
// ��ģ����� (m_client, m_cfg, m_device, m_rtsp, m_mapWidget)
// ���� MainWindow Ϊ�������� Qt �������Զ�����
// ����ǰֹͣ RTSP �̣߳�����ֹͣ��־�� FFmpeg �жϻص���ʹ����ٷ���
//============================================================================
MainWindow::~MainWindow()
{
    if (m_sysParamTimer)
        m_sysParamTimer->stop();
    disconnect(m_presenter->tcpClient(), nullptr, this, nullptr);
    if (m_presenter->videoStream()) {
        ui->videoWidget->clearFrame();
        m_presenter->videoStream()->closeStream();
        m_presenter->videoStream()->wait(2000);
    }
    delete m_pipDialog;
    delete ui;
}

//============================================================================
// ��������ť
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
    dlg.setWindowTitle(QStringLiteral("�ر���ʾ"));
    dlg.setFixedSize(300, 160);
    dlg.setWindowFlags((dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint));

    auto *layout = new QVBoxLayout(&dlg);

    // Radio ��ť�У�������˳������Ҷ�����С��������
    auto *radioLayout = new QHBoxLayout();
    auto *radioExit = new QRadioButton(QStringLiteral("�˳�����"), &dlg);
    auto *radioMin = new QRadioButton(QStringLiteral("��С��������"), &dlg);
    radioMin->setChecked(true);
    radioLayout->addWidget(radioExit);
    radioLayout->addStretch();
    radioLayout->addWidget(radioMin);
    layout->addLayout(radioLayout);

    // �ײ��У���סѡ����+ ȷ�ϣ��ң�
    auto *bottomLayout = new QHBoxLayout();
    auto *cbRemember = new QCheckBox(QStringLiteral("��ס����ѡ��"), &dlg);
    bottomLayout->addWidget(cbRemember);
    bottomLayout->addStretch();
    auto *btnConfirm = new QPushButton(QStringLiteral("ȷ��"), &dlg);
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
// ϵͳ����
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
                                QStringLiteral("��������С����ϵͳ����"),
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
        ui->videoWidget->clearFrame();
        m_presenter->videoStream()->closeStream();
    }
    if (m_presenter->tcpClient()->isConnected())
        m_presenter->tcpClient()->disconnectDevice();
    qApp->quit();
}

//============================================================================
// ������ť
//============================================================================

void MainWindow::on_btnNavMonitor_clicked()  { /* ��ǰҳ�� */ }
void MainWindow::on_btnNavPlayback_clicked() { /* Ԥ�� */ }
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

    // ���Э���������´򿪴���
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

    // ���� PTZ ת������
    if (m_cfg->serialServerEnabled()) {
        m_presenter->ptzForwarder()->start(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
    } else {
        // Ӧ��Ҳֹͣ������Ŀǰû��ֹͣ���������� start �㹻�����ǵ��δ�����
        // Let's assume PtzForwarder doesn't have stop or it doesn't matter for now.
    }
}

//============================================================================
// changeEvent - ����״̬�仯ʱ������󻯰�ťͼ��
//============================================================================

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        bool max = isMaximized();
        ui->btnMenu_Max->setIcon(QIcon(max
            ? QStringLiteral(":/qss/blacksoft/restore.png")
            : QStringLiteral(":/qss/blacksoft/maximize.png")));
        ui->btnMenu_Max->setToolTip(max
            ? QString::fromUtf8("���ڻ�")
            : QString::fromUtf8("���"));
    }
    QMainWindow::changeEvent(event);
}

//============================================================================
// nativeEvent - ���� Windows ��Ϣʵ���Զ��������
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
                // ���ʱ Windows �����ı���Ӳ��ɼ��߿�������ƫ��
                // �����߿���ʹ�ͻ�������������
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
            // GET_X_LPARAM ���������������꣬�� / devicePixelRatioF() תΪ Qt �߼�����
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
                    if (child == ui->btnMenu_Min || child == ui->btnMenu_Max || child == ui->btnMenu_Close
                        || child == ui->btnNavMonitor || child == ui->btnNavPlayback
                        || child == ui->btnNavLog || child == ui->btnNavSettings
                        || child == ui->lineEditIp || child == ui->btnConnect
                        || child == ui->btnCancelConnect || child == ui->lineEditRtsp
                        || child == ui->btnVideoConnect || child == ui->btnVideoDisconnect
                        || child == ui->btnMapToggle)
                        { *result = HTCLIENT; return true; }
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
// setupUiStyles - ���ز�Ӧ�� QSS ��ʽ��
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

    // QScrollArea viewport Ĭ�ϼ̳�ϵͳ����ɫ��ǿ����Ϊ����
    if (ui->scrollAreaControl) {
        ui->scrollAreaControl->viewport()->setAutoFillBackground(true);
        QPalette pal = ui->scrollAreaControl->viewport()->palette();
        pal.setColor(QPalette::Window, QColor(0x44, 0x44, 0x44));
        ui->scrollAreaControl->viewport()->setPalette(pal);
    }
}

//============================================================================
// on_btnConnect_clicked - ����/�Ͽ��豸��ť
// ������ʱ���Ϊ�Ͽ���δ����ʱ��ȡ IP �Ͷ˿ڷ��� TCP ����
//============================================================================
void MainWindow::on_btnConnect_clicked()
{
    if (m_presenter->tcpClient()->isConnected()) {
        m_presenter->tcpClient()->disconnectDevice();
    } else {
        if (!m_cfg->turntableIpEnabled()) {
             ui->statusbar->showMessage(QString::fromUtf8("ת̨IP�����ѽ���"), 3000);
             return;
        }
        QString ip = ui->lineEditIp->text();
        m_presenter->tcpClient()->connectToDevice(ip, 8089);
        ui->btnConnect->setText(QString::fromUtf8("������..."));
        ui->btnConnect->setEnabled(false);
        ui->btnCancelConnect->setVisible(true);
    }
}

//============================================================================
// on_btnCancelConnect_clicked - ȡ�����ڽ��е�����
// ֱ�ӶϿ� TCP ���Ӳ��ָ���ť״̬
//============================================================================
void MainWindow::on_btnCancelConnect_clicked()
{
    m_presenter->tcpClient()->disconnectDevice();
    ui->btnConnect->setText(QString::fromUtf8("�����豸"));
    ui->btnConnect->setEnabled(true);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("��ȡ������"), 3000);
}

//============================================================================
// on_btnVideoConnect_clicked - ���� RTSP ��Ƶ��
// ��������ȡ RTSP URL �󽻸� RtspThread ��������
//============================================================================
void MainWindow::on_btnVideoConnect_clicked()
{
    QString url = ui->lineEditRtsp->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "RTSP", "������ RTSP ��ַ");
        return;
    }
    m_rtspEverOpened = true;
    m_presenter->videoStream()->openStream(url);
    ui->btnVideoConnect->setEnabled(false);
    ui->btnVideoConnect->setText(QString::fromUtf8("������..."));
    ui->statusbar->showMessage(QString::fromUtf8("�������� RTSP ��Ƶ��..."));
}

//============================================================================
// on_btnVideoDisconnect_clicked - �Ͽ� RTSP ��Ƶ��
// ֹͣ�����̡߳������Ƶ���桢�ָ���ť״̬
//============================================================================
void MainWindow::on_btnVideoDisconnect_clicked()
{
    ui->videoWidget->clearFrame();
    ui->videoWidget->repaint();
    m_presenter->videoStream()->closeStream();
    ui->btnVideoConnect->setEnabled(true);
    ui->btnVideoConnect->setText(QString::fromUtf8("����"));
    ui->statusbar->showMessage(QString::fromUtf8("��Ƶ�ѶϿ�"), 3000);
}

//============================================================================
// onRtspFrame - �յ�һ֡ RTSP ��Ƶͼ��
// �������� QImage ���ݸ� VideoWidget ������Ⱦ
//============================================================================
void MainWindow::onRtspFrame(const QImage &frame)
{
    ui->videoWidget->setFrame(frame);
}

//============================================================================
// onRtspOpened - RTSP ��Ƶ���ɹ���
// ���°�ť�ı���״̬����ʾ
//============================================================================
void MainWindow::onRtspOpened()
{
    ui->btnVideoConnect->setEnabled(false);
    ui->btnVideoConnect->setText(QString::fromUtf8("������"));
    ui->statusbar->showMessage(QString::fromUtf8("RTSP ��Ƶ������"), 3000);
}

//============================================================================
// onRtspError - RTSP ��Ƶ��������
// ������桢�ָ���ť������״̬����ʾ������Ϣ
//============================================================================
void MainWindow::onRtspError(const QString &msg)
{
    ui->videoWidget->clearFrame();
    if (m_presenter->videoStream()->isRunning()) {
        // �̻߳�������˵�����Զ������У����ְ�ť��"������..."״̬
        ui->btnVideoConnect->setText(QString::fromUtf8("������..."));
        ui->statusbar->showMessage(msg.isEmpty()
            ? QString::fromUtf8("RTSP �Ͽ�����������...")
            : QString::fromUtf8("RTSP ����ʧ�ܣ���������..."));
    } else {
        // �߳����˳�����ť�ָ�"����"���û��ֶ�����
        ui->btnVideoConnect->setEnabled(true);
        ui->btnVideoConnect->setText(QString::fromUtf8("����"));
        ui->statusbar->showMessage(msg);
    }
}

//============================================================================
// onVideoSelection - �û�����Ƶ�����ϵĿ�ѡ����
// ����ѡ�������������߷��͸��豸�����ڿ�ѡ����ģʽ
// cx, cy Ϊ��ѡ���������������꣬pw, ph Ϊ����
//============================================================================
void MainWindow::onVideoSelection(int cx, int cy, int pw, int ph)
{
    int wm = ui->comboWorkMode->currentIndex();
    if (wm != 3 && wm != 4) {
        ui->statusbar->showMessage(QString::fromUtf8("���ڵ�ѡ���ٻ��ѡ����ģʽ��֧�ֿ�ѡ"), 3000);
        return;
    }

    if (wm == 3) {
        ui->statusbar->showMessage(
            QString::fromUtf8("��ѡ����: ��������(%1,%2)")
                .arg(cx).arg(cy));
        if (!requireConnected()) return;
        m_presenter->motorController()->setPointTrack(cx, cy);
    } else {
        ui->statusbar->showMessage(
            QString::fromUtf8("��ѡ����: ��������(%1,%2) ��%3��%4")
                .arg(cx).arg(cy).arg(pw).arg(ph));
        if (!requireConnected()) return;
        m_presenter->motorController()->setBoxTrack(cx, cy, pw, ph);
    }
}

//============================================================================
// onDeviceConnected - �豸���ӳɹ��ص�
// ���°�ť��ʽΪ��ɫ"�Ͽ�����"���Զ���ѯ�豸��ǰͼ�����
//============================================================================
void MainWindow::onDeviceConnected()
{
    ui->btnConnect->setText(QString::fromUtf8("�Ͽ�����"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", "connected");
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("�����ӵ��豸"), 3000);

    m_workModeInitialized = false;
    m_displayModeInitialized = false;
    m_algoModelInitialized = false;
    // ���ӳɹ����Զ�����һ��ͼ��������Ա� UI ���豸״̬ͬ��
    m_presenter->motorController()->queryImageParams();

    // ���Ӻ�ͬ�����л��濪��״̬��ȷ���豸�� UI һ��
    m_presenter->motorController()->setDigitalZoom(m_cfg->digitalZoomEnabled());
    m_presenter->motorController()->setAutoZoom(m_cfg->autoZoomEnabled());
    m_presenter->motorController()->setCaptureUpload(m_cfg->captureUploadEnabled());
    m_presenter->motorController()->posReset(m_cfg->posResetEnabled());

    // ���ϵͳ������ʱ�·�
    m_sysParamTimer->start();

    // �״������豸ʱ�Զ��� RTSP���������ٸ����û�����
    if (!m_rtspEverOpened) {
        QString rtspUrl = ui->lineEditRtsp->text().trimmed();
        if (!rtspUrl.isEmpty()) {
            m_rtspEverOpened = true;
            ui->btnVideoConnect->setEnabled(false);
            ui->btnVideoConnect->setText(QString::fromUtf8("������..."));
            m_presenter->videoStream()->openStream(rtspUrl);
        }
    }
}

//============================================================================
// onDeviceDisconnected - �豸�Ͽ��ص�
// �ָ����Ӱ�ť�ĳ�ʼ���
//============================================================================
void MainWindow::onDeviceDisconnected()
{
    ui->btnConnect->setText(QString::fromUtf8("�����豸"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", QVariant());
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("�豸�ѶϿ�"), 3000);

    // ֹͣϵͳ������ʱ�·�
    m_sysParamTimer->stop();
}

// 200ms ���ڲ�ѯϵͳ������������״̬ʱ�·���
void MainWindow::onSysParamTimerTimeout()
{
    if (m_presenter->tcpClient()->isConnected()) m_presenter->motorController()->queryImageParams();
}

//============================================================================
// onErrorOccurred - ���Ӵ�����
// ������ʱ������ʾ����������ֻ��״̬����ʾ�������Զ�����
//============================================================================
void MainWindow::onErrorOccurred(const QString& errorMsg)
{
    if (ui->btnConnect->property("state").toString() == QStringLiteral("reconnecting")) {
        ui->statusbar->showMessage(QString::fromUtf8("����ʧ�ܣ�%1").arg(errorMsg), 3000);
        return;
    }

    ui->btnConnect->setText(QString::fromUtf8("�����豸"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setProperty("state", QVariant());
    refreshStyle(ui->btnConnect);
    ui->btnCancelConnect->setVisible(false);
    QMessageBox::warning(this, QString::fromUtf8("���Ӵ���"), errorMsg);
}

//============================================================================
// onAckReceived - �����豸���ص� ACK Ӧ��
// ACK ״̬��:
//   0 = ִ������, 1 = ��������, 2 = Э�����ݴ���
// SetDigitalZoom/SetCaptureState/SetPosReset ������ָ���豸�̶��� 1�����ɹ�����
//============================================================================
void MainWindow::onAckReceived(quint8 statusCode)
{
    if (statusCode == 0) {
        ui->statusbar->showMessage(QString::fromUtf8("[ACK] ָ��ִ�гɹ�"), 3000);
        return;
    }
    if (statusCode == 1) {
        // SetDigitalZoom/SetCaptureState/SetPosReset �豸�̶��� 1����Ϊ�ɹ�
        if (m_lastAckFrameType == FrameType::SetDigitalZoom
            || m_lastAckFrameType == FrameType::SetCaptureState
            || m_lastAckFrameType == FrameType::SetPosReset) {
            ui->statusbar->showMessage(QString::fromUtf8("[ACK] ָ��ִ�гɹ�"), 3000);
            return;
        }
        ui->statusbar->showMessage(QString::fromUtf8("[ACK] ��������"), 3000);
        return;
    }
    QString msg;
    switch (statusCode) {
    case 2: msg = QString::fromUtf8("Э�����ݴ���"); break;
    default: msg = QString::fromUtf8("δ֪״̬��: %1").arg(statusCode);
    }
    ui->statusbar->showMessage(QString::fromUtf8("[ACK] %1").arg(msg), 3000);
}

//============================================================================
// onJsonReceived - �յ��豸���͵� JSON ����֡
// ������ JSON �ĵ����� updateStatusFromJson ���н����� UI ˢ��
//============================================================================
void MainWindow::onJsonReceived(const QJsonObject& doc)
{
    updateStatusFromJson(doc);
}

//============================================================================
// onImageSnapped - �豸ץ��ͼ��ص�
// �� JPEG ���ݱ��浽 snapshots Ŀ¼���ļ���Ϊ yyyyMMdd_HHmmss_zzz.jpg
// ״̬����ʾ����·����ͼ���ڻ����е�λ����Ϣ
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
            QString::fromUtf8("�ѱ���ץ��: %1  λ��: (%2,%3 %4x%5)")
                .arg(path)
                .arg(location.x()).arg(location.y())
                .arg(location.width()).arg(location.height()),
            5000);
    }
}

//============================================================================
// updateStatusFromJson - JSON ֡������ UI ״̬���£����ķ�����
// ���� ControlType �ֶηַ����������������ͣ�
//   AIInfo     �� ʶ��/���ٽ�� (Object �б���Ѱ���������״̬��)
//   ZoomInfo   �� ��ͷ�䱶��Ϣ��GPS ���ꡢ��̨�Ƕȡ�������
//   ImageSetting �� ͼ����� (�ֱ���/����/����/����ģʽ/��ʾģʽ/�㷨ģ��)
//============================================================================
void MainWindow::updateStatusFromJson(const QJsonObject& doc)
{
    QString controlType = doc.value("ControlType").toString();
    CameraConfig& cam = m_cfg->cam();

    //==========================================================================
    // 1) AIInfo - AI ʶ������ٽ��֡
    //==========================================================================
    if (controlType == "AIInfo") {
        m_lastAiInfoTime = QDateTime::currentDateTime();
        int workMode = doc.value("WorkMode").toInt();
        int count = doc.value("ObjectCount").toInt();

        if (workMode == 1) {
            //==================================================================
            // ʶ��ģʽ (WorkMode=1)��
            // ���� Object �ֵ䣬��ÿ��Ŀ��� ID/���/����/����λ��/�Ѱ���
            // ����ʶ������� tableIdentify
            //==================================================================
            ui->lblIdentifyCount->setText(QString::fromUtf8("Ŀ������: %1").arg(count));
            ui->tableIdentify->setRowCount(0);  // ��վ����ݣ��������

            // ���ݵ�ǰ��ʾģʽ�ж�ʹ�ÿɼ��⻹�Ǻ������
            // combo ����: 0=��ͼ�ɼ���, 1=����, 2=�ɼ���, 3=�ں�, 4=��ͼ����
            bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
            double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                              : cam.irMinFocal * m_currentIrZoom;
            int halfW = (isVis ? m_currentResX : cam.irResX) / 2;
            int halfH = (isVis ? m_currentResY : cam.irResY) / 2;

            // Object �ֶ���һ���ֵ䣬key ΪĿ�� ID��value ΪĿ������
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

                        // ����Ŀ����������ڻ������ĵ��Ѱ����������ȣ�
                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        QString miss = GeoCalculator::missMradStr(cx - halfW, cy - halfH, px, fl);
                        ui->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
                    }
                }
            }
        }

        // ʶ��ģʽ�����ģʽ����Ҫ���µ�ͼ�ϵ�Ŀ����
        if ((workMode == 1) || (workMode >= 2 && workMode <= 4))
            updateMapTargets(doc, workMode);

        //==================================================================
        // ����ģʽ (WorkMode=2~4)��
        //   2 = �Զ�����, 3 = ��ѡ����, 4 = ����/��ѡ����
        // ��ʾ����״̬��Ŀ�� ID����𡢾��롢�Ƕȡ����ؿ��Ѱ���
        // Class=0xB1 ��ʾ����������Ϊ��ʧ
        //==================================================================
        if (workMode >= 2 && workMode <= 4) {
            bool hasObj = doc.contains("Object") && doc.value("Object").isObject()
                          && !doc.value("Object").toObject().isEmpty();

            if (hasObj) {
                QJsonObject objMap = doc.value("Object").toObject();
                QJsonObject obj = objMap.begin().value().toObject();
                int cls = obj.value("Class").toInt();

                bool locked = (cls == 0xB1);
                QString statusText = locked ? QString::fromUtf8("������") : QString::fromUtf8("��ʧ");
                QString statusFull = QString::fromUtf8("״̬: %1").arg(statusText);
                ui->lblTrackStatus->setText(statusFull);
                ui->lblTrackStatus->setProperty("state", locked ? "locked" : "missed");
                refreshStyle(ui->lblTrackStatus);

                if (obj.contains("Distance")) {
                    double rawDist = obj.value("Distance").toDouble(0);
                    if (rawDist > 0)
                        ui->trackDistance->setText(QString::number(rawDist, 'f', 1) + QStringLiteral(" m"));
                    // rawDist==0: ���� calcVisualDistance ���õĹ���ֵ
                } else
                    ui->trackDistance->clear();

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    int cx = (l + r2) / 2, cy = (t + b) / 2;
                    int pw = r2 - l, ph = b - t;
                    ui->trackPos->setText(QString("(%1,%2) %3��%4").arg(cx).arg(cy).arg(pw).arg(ph));

                    // �����Ѱ���������ƫ�� �� ��Ԫ�ߴ� / ���� �� ������
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
                // ��Ŀ�꣺��ʾ"δ����"��������и����ֶ�
                ui->lblTrackStatus->setText(QString::fromUtf8("״̬: δ����"));
                ui->lblTrackStatus->setProperty("state", "nolock");
                refreshStyle(ui->lblTrackStatus);
                ui->trackPos->clear();
                ui->trackMissDistance->clear();
                ui->trackDistance->clear();
            }
        }

    //==========================================================================
    // 2) ZoomInfo - ��ͷ�������豸״̬֡
    // ���±䱶���ʡ�GPS ���ꡢ�߶ȡ������ࡢ��̨ˮƽ/��ֱ��
    // ͬʱ������ͷͳ����Ϣ�������ͼ�豸λ�ø���
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

        ui->statPanAngle->setText(QString::number(rawPan, 'f', 1) + QStringLiteral("��"));
        m_currentTilt = rawTilt;
        ui->statTiltAngle->setText(QString::number(rawTilt, 'f', 1) + QStringLiteral("��"));

        updateLensStats();
        updateMapDevicePosition(doc);

    //==========================================================================
    // 3) ImageSetting - ͼ���������֡
    // �豸�������ͻ���Ӧ��ѯ�����·ֱ���/����/����/����ģʽ/��ʾģʽ/�㷨
    // �������豸��ǰֵͬ�� UI ������ͬʱ���� m_updatingFromDevice ��־
    // ��ֹ UI �仯�ٴδ����豸ָ�������ѭ��
    //==========================================================================
    } else if (controlType == "ImageSetting") {
        // ͼ��ֱ���ӳ���
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

        // ͼ������
        ui->paramBitrate->setText(QString("%1 Kb/s").arg(doc.value("ImageBit").toInt()));

        // �����ʽӳ���
        static const char* codecMap[] = {"H264", "H265"};
        int codec = doc.value("ImageCode").toInt();
        ui->paramCodec->setText(codec >= 0 && codec < 2 ? codecMap[codec] : QString::number(codec));

        // ����ģʽӳ���
        static const char* wmMap[] = {"�ر�AI", "ʶ��", "�Զ�����", "��ѡ����", "����/��ѡ����"};
        int wm = doc.value("WorkMode").toInt();
        ui->paramWorkMode->setText(wm >= 0 && wm < 5 ? QString::fromUtf8(wmMap[wm]) : QString::number(wm));
        m_previousWorkMode = wm;

        // ��ʾ����ӳ��� (PIP = Picture-in-Picture)
        static const char* pipMap[] = {"��ͼ�ɼ���", "����", "�ɼ���", "�ں�", "��ͼ����"};
        int pipRaw = doc.value("PipShow").toInt();
        int comboIdx = DeviceController::pipShowToComboIndex(pipRaw);
        ui->paramPipShow->setText(comboIdx >= 0 && comboIdx < 5 ? QString::fromUtf8(pipMap[comboIdx]) : QString::number(pipRaw));

        // �㷨ģ�ͱ���: �߶�(������)��10 + �Ͷ�(ʶ������)
        int model = doc.value("Model").toInt();
        int high = model / 10;
        int low  = model % 10;
        static const char* highMap[] = {"�ɼ���", "����"};
        static const char* lowMap[]  = {"", "", "�˳�ʶ��", "��ʶ��", "���˻�ʶ��", "�ɻ�ֱ����ʶ��", "��ʶ��"};
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

        // ͬ�� UI �������豸��ǰֵ��ͬʱ�����źŵݹ�
        m_updatingFromDevice = true;
        // �״�����ʱͬ���㷨ģ�������򣬺������ٸ����û�ѡ��
        if (!m_algoModelInitialized) {
            m_currentAlgoModel = model;
            // �߶� = ���������� (0=�ɼ���, 1=����) �� comboAlgoModel1
            if (high >= 0 && high < ui->comboAlgoModel1->count())
                ui->comboAlgoModel1->setCurrentIndex(high);
            // �Ͷ� = ʶ������ (2-6 �� comboAlgoModel2 ���� 0-4)
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
        // �״�����ʱͬ������ģʽ�����򣬺������ٸ����û�ѡ��
        if (!m_workModeInitialized && wm >= 0 && wm < ui->comboWorkMode->count()) {
            ui->comboWorkMode->setCurrentIndex(wm);
            m_workModeInitialized = true;
        }
        m_updatingFromDevice = false;
    }
}

//============================================================================
// updateLensStats - ���¾�ͷͳ������
// ���ݵ�ǰ�䱶���ʼ���ɼ��������ģ�
//   - ��ǰ���� (��С���� �� ����)
//   - ˮƽ�ӳ��� (HFOV): 2 �� arctan(��������� / (2 �� ����))
// ��������� = ��Ԫ�ߴ� �� ˮƽ�ֱ��� (��λ����Ϊ mm)
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
    ui->statFovVis->setText(QString::number(visHfov * kRad2Deg, 'f', 2) + QStringLiteral("��"));

    ui->statZoomIR->setText(QString::number(m_currentIrZoom, 'f', 2) + QStringLiteral("x"));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 2) + QStringLiteral(" mm"));
    ui->statFocusIR->clear();

    double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));
    ui->statFovIR->setText(QString::number(irHfov * kRad2Deg, 'f', 2) + QStringLiteral("��"));
}

//============================================================================


//============================================================================
// requireConnected - δ����ʱ������ʾ������ false
// ������Ҫ�����豸����ִ�е� UI ������Ӧ�ȵ��ô˺���
//============================================================================
bool MainWindow::requireConnected()
{
    if (!m_presenter->tcpClient()->isConnected()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("��ʾ"));
        msgBox.setText(QStringLiteral("�������豸"));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
        msgBox.exec();
        return false;
    }
    return true;
}

// ������Ƿ����
// MODBUS-RTU Э��ʱ�贮���Ѵ򿪣�Pelco-D ֱ������
bool MainWindow::requireMotorReady()
{
    if (m_cfg->motorProtocol() == "MODBUS-RTU") {
        if (m_cfg->motorCommandChannel() == "串口" && !m_presenter->motorController()->isMotorSerialOpen()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("��ʾ"));
            msgBox.setText(QStringLiteral("�������δ�򿪣���������������"));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
            msgBox.exec();
            return false;
        }
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        if (!m_presenter->motorController()->isMotorTcpOpen()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(QStringLiteral("��ʾ"));
            msgBox.setText(QStringLiteral("��� TCP �������ӻ�����ʧ�ܣ���������"));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setStyleSheet("QPushButton { min-width: 80px; margin: 5px; }");
            // ������������������ȥ��������ֱ�� return false
            // ���������Ҫǿ�ƵĻ����� return false; 
            // ������ DeviceController �����Ѿ��� sendMotorTcpV4 ʱ����Ͽ����Զ���������һ��
        }
    }
    return true;
}

// ���µ�����ư�ť״̬
void MainWindow::updateMotorButtons()
{
    bool isModbus = (m_cfg->motorProtocol() == "MODBUS-RTU");
    bool isTcp = (m_cfg->motorProtocol() == "STM32-TCP-V4.0");
    bool isPelco = (m_cfg->motorProtocol() == "Pelco-D");
    
    // ֻ�� Modbus �� TCP ȫ���ܿ��ã�Pelco-D ��������ˢ
    
    bool othersEnabled = !isPelco;
    ui->btnWiperLeft->setEnabled(othersEnabled);
    ui->btnWiperRight->setEnabled(othersEnabled);
    ui->btnWiperZeroCalib->setEnabled(othersEnabled);
    ui->btnWiperMode->setEnabled(othersEnabled);
    
    // ��/����ģʽ���� STM32-TCP-V4.0 ����Ч�����������ϣ�� Modbus Ҳ��Ԥ������Ե���
    // �����ĵ���action 6/7 ���� V4.0 TCP �ӿ�
    ui->btnWiperSilent->setEnabled(isTcp);

    if (isModbus && m_presenter->motorController()->isMotorSerialOpen()) {
        m_presenter->motorController()->motorCheckMode();
    }
}

//============================================================================
// on_comboWorkMode_currentIndexChanged - ����ģʽ�������л�
//   0 = �ر�AI, 1 = Ŀ��ʶ��, 2 = �Զ�����, 3 = ��ѡ����, 4 = ��ѡ����
//============================================================================
void MainWindow::on_comboWorkMode_currentIndexChanged(int index)
{
    // �ǵ�ѡ/��ѡ����ģʽʱ��ֹ����ѡ������ UI ״̬�����漰�豸ָ�
    ui->videoWidget->setSelectionEnabled(index == 3 || index == 4);

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
// on_btnPtzMoveTo_clicked - ��̨ת��ָ���Ƕ�
//============================================================================
void MainWindow::on_btnPtzMoveTo_clicked()
{
    if (!requireConnected()) return;

    bool panOk = false;
    bool tiltOk = false;
    double pan = ui->editTargetPan->text().toDouble(&panOk);
    double tilt = ui->editTargetTilt->text().toDouble(&tiltOk);

    if (panOk && tiltOk) {
        m_presenter->motorController()->ptzMoveTo(pan, tilt);
    } else {
        QMessageBox::warning(this, "�������", "��������Ч��ˮƽ�ʹ�ֱ�Ƕ�ֵ��");
    }
}

//============================================================================
// on_btnPtzMoveToGps_clicked - ��̨ת����ָ����γ�ȸ߶�
//============================================================================
void MainWindow::on_btnPtzMoveToGps_clicked()
{
    if (!requireConnected()) return;

    QString lonStr = ui->editTargetLon->text().trimmed();
    QString latStr = ui->editTargetLat->text().trimmed();
    QString altStr = ui->editTargetAlt->text().trimmed();

    if (lonStr.isEmpty() || latStr.isEmpty()) {
        QMessageBox::warning(this, "�������", "������Ŀ��ľ�γ�Ⱥ͸߶ȡ�");
        return;
    }

    double targetLon = parseCoord(lonStr);
    double targetLat = parseCoord(latStr);
    double targetAlt = altStr.toDouble();

    double devLat = parseCoord(ui->statLatitude->text());
    double devLon = parseCoord(ui->statLongitude->text());
    double devAlt = m_deviceHeight;

    if (devLat == 0 && devLon == 0) {
        QMessageBox::warning(this, "״̬����", "��ǰ�豸 GPS δ֪���޷�����Ŀ��Ƕȡ�");
        return;
    }

    double pan = bearing(devLat, devLon, targetLat, targetLon);
    double dist = haversineDistance(devLat, devLon, targetLat, targetLon);

    double tilt = 0;
    if (dist > 0.001) { 
        tilt = -qRadiansToDegrees(qAtan2(targetAlt - devAlt, dist));
    }

    m_presenter->motorController()->ptzMoveTo(pan, tilt);
    ui->statusbar->showMessage(QString("ת�� GPS: ��λ=%1�� ����=%2��").arg(pan, 0, 'f', 1).arg(tilt, 0, 'f', 1), 3000);
}

//============================================================================
// on_btnPanZeroCalib_clicked - ˮƽ���궨
//============================================================================
void MainWindow::on_btnPanZeroCalib_clicked()
{
    if (!requireConnected()) return;
    
    if (QMessageBox::question(this, "���궨", "ȷ�Ͻ���ǰ��̨ˮƽ�͸���λ�ñ궨Ϊ 0 �ȣ�") == QMessageBox::Yes) {
        if (m_cfg->softwarePtzCalibrationEnabled()) {
            // ������ģ�⴮�ڷ�������ʹ�����ƫ��
            QString panStr = ui->statPanAngle->text();
            panStr.remove("��");
            double displayedPan = panStr.toDouble();

            QString tiltStr = ui->statTiltAngle->text();
            tiltStr.remove("��");
            double displayedTilt = tiltStr.toDouble();

            double oldPanOffset = m_cfg->ptzPanOffset();
            double oldTiltOffset = m_cfg->ptzTiltOffset();

            double newPanOffset = displayedPan + oldPanOffset;
            while (newPanOffset >= 360.0) newPanOffset -= 360.0;
            while (newPanOffset < 0) newPanOffset += 360.0;

            double newTiltOffset = oldTiltOffset - displayedTilt;
            while (newTiltOffset > 180.0) newTiltOffset -= 360.0;
            while (newTiltOffset <= -180.0) newTiltOffset += 360.0;

            m_cfg->setPtzPanOffset(newPanOffset);
            m_cfg->setPtzTiltOffset(newTiltOffset);
            m_cfg->save();

            m_presenter->ptzForwarder()->setOffsets(newPanOffset, newTiltOffset);
            m_presenter->ptzForwarder()->flushZeroPosition();

            ui->statPanAngle->setText("0.0��");
            ui->statTiltAngle->setText("0.0��");
            ui->statusbar->showMessage("���궨(���ƫ��)�ѱ���", 3000);
        } else {
            // δ����ģ�⴮�ڷ�������ֱ��ͨ�� PELCO-D ͸���궨ָ��
            m_presenter->motorController()->ptzSetZero();
            ui->statusbar->showMessage("���궨ָ��(Pelco-D)���·�", 3000);
            // ���ﲻǿ�Ƹ� UI���ú����豸�����ϱ����½Ƕ���ˢ�� UI
        }
    }
}


//============================================================================
//============================================================================
// on_comboAlgoModel1/2_currentIndexChanged - �㷨ģ���������л�
// �� m_updatingFromDevice �����������豸�ش�ʱ�ظ��·�ָ��
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
// on_comboDisplayMode_currentIndexChanged - ��ʾģʽ�������л�
// �л�ʱ�·���ʾģʽ���ָ���ͷĿ������ʾģʽ�Զ��ж�
//============================================================================
void MainWindow::on_comboDisplayMode_currentIndexChanged(int index)
{
    if (!requireConnected()) { ui->comboDisplayMode->blockSignals(true); ui->comboDisplayMode->setCurrentIndex(m_previousDisplayMode); ui->comboDisplayMode->blockSignals(false); return; }
    if (m_updatingFromDevice) return;
    // ������ʾģʽ�Զ��л��㷨ģ�ͣ�0/2/3���ɼ���ģ�ͣ�1/4������ģ��
    // ֱ���·������� queryImageParams�������豸���ؾ����ݸ�����ʾģʽ
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
    // �Ӻ�����ʾģʽ�������� setAlgoModel ���������豸����
    QTimer::singleShot(150, this, [this]() {
        if (m_presenter->tcpClient()->isConnected()) {
            int idx = ui->comboDisplayMode->currentIndex();
            m_presenter->motorController()->setDisplayMode(idx);
        }
    });
}

//============================================================================
// on_comboLensTarget_currentIndexChanged - ��ɾ������ͷĿ������ʾģʽ�Զ��ж�
//============================================================================

//============================================================================
// on_btnSetLocation_clicked - �ֶ������豸��γ��
// ��������ȡ��γ���ַ�����ֱ���·����豸��д GPS ��Ϣ
//============================================================================
void MainWindow::on_btnSetLocation_clicked()
{
    QString latStr = ui->editSetLat->text().trimmed();
    QString lonStr = ui->editSetLon->text().trimmed();

    if (latStr.isEmpty() || lonStr.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("�������"),
                             QString::fromUtf8("����д�����ľ�γ�Ȳ���"));
        return;
    }

    if (!requireConnected()) return;

    double latNum = parseCoord(latStr);
    double lonNum = parseCoord(lonStr);

    QString altStr = ui->editSetHeight->text().trimmed();
    if (!altStr.isEmpty()) {
        m_deviceHeight = altStr.toDouble();
        ui->statHeight->setText(QString::number(m_deviceHeight, 'f', 1) + QStringLiteral(" m"));
    }

    QString strictLat = QString::asprintf("%.7f%s", qAbs(latNum), latNum >= 0 ? "N" : "S");
    QString strictLon = QString::asprintf("%.7f%s", qAbs(lonNum), lonNum >= 0 ? "E" : "W");

    m_presenter->motorController()->setLocation(strictLat, strictLon);
    ui->statusbar->showMessage(QString::fromUtf8("���·���γ��"), 3000);
}

//============================================================================
// on_btnGetImageParams_clicked - ��ѯ�豸��ǰͼ�����
// �豸���� ImageSetting ����֡�ظ������� updateStatusFromJson ���� UI
//============================================================================
void MainWindow::on_btnGetImageParams_clicked()
{
    if (!requireConnected()) return;
    m_presenter->motorController()->queryImageParams();
    ui->statusbar->showMessage(QString::fromUtf8("�ѷ��Ͳ�����ѯ����"), 3000);
}

//============================================================================
// parseCoord - �����ַ����������ߺ���
// �������׺�ľ�γ�ȸ�ʽ������ "39.9042N" �� 39.9042, "116.4074E" �� 116.4074
// ��γ(S)������(W)���ظ�ֵ�����޺�׺��ֱ�ӷ�����ֵ
//============================================================================
static double parseCoord(const QString& s) {
    QString t = s.trimmed().toUpper();
    char suf = 0;
    if (!t.isEmpty()) {
        QChar c = t.at(t.size() - 1);
        if (c == 'N' || c == 'S' || c == 'E' || c == 'W') {
            suf = c.toLatin1(); t.chop(1);
        }
    }
    bool ok = false;
    double v = t.toDouble(&ok);
    if (!ok) return 0.0;
    return (suf == 'S' || suf == 'W') ? -v : v;
}

//============================================================================
// updateMapDevicePosition - ���µ�ͼ�ϵ��豸λ�����ӳ���
// �� ZoomInfo JSON ֡�н��� GPS����̨�Ƕȡ�����������ݣ�
// ���㵱ǰ��ͷ��ˮƽ/��ֱ�ӳ��ǣ����Ƶ���ͼ�ؼ���
//
// �ӳ��Ǽ��㣺
//   HFOV = 2 �� arctan(���������_mm / (2 �� ����_mm))
//   VFOV = HFOV �� 9/16 (�ٶ� 16:9 ��������߱�)
// ��������� = ��Ԫ�ߴ� �� ˮƽ�ֱ��� / 1000
//============================================================================
void MainWindow::updateMapDevicePosition(const QJsonObject& doc)
{
    QString latStr = doc.value("Latitude").toString();
    QString lonStr = doc.value("Longitude").toString();
    double lat = parseCoord(latStr);
    double lon = parseCoord(lonStr);
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

    // ����ɼ����ӳ���
    CameraConfig& cam = m_cfg->cam();
    double visSensorW = cam.visPixelSize * cam.visResX / 1000.0;
    double visFocal = cam.visMinFocal * m_currentVisZoom;
    double visHfov = 2.0 * qAtan(visSensorW / (2.0 * visFocal)) * 180.0 / M_PI;
    double visVfov = visHfov * cam.visResY / cam.visResX;

    // ��������ӳ���
    double irSensorW = cam.irPixelSize * cam.irResX / 1000.0;
    double irFocal = cam.irMinFocal * m_currentIrZoom;
    double irHfov = 2.0 * qAtan(irSensorW / (2.0 * irFocal)) * 180.0 / M_PI;
    double irVfov = irHfov * cam.irResY / cam.irResX;

    // �ɼ����ӳ��� 4km����ɫ���������ӳ��� 2km����ɫ��
    m_mapWidget->setVisFov(lat, lon, pan, tilt, visHfov, visVfov, 4000);
    m_mapWidget->setIrFov(lat, lon, pan, tilt, irHfov, irVfov, 2000);
    m_mapWidget->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, rangeEstimated);
}

//============================================================================
// pixelToGps - ��������ת GPS ��������
// ��ͼ����ĳ���ص� (pixelX, pixelY) ӳ�䵽��ʵ�����γ�ȡ�
// ���Ĳ��裺
//   1. ����ƫ�� �� �Ƕ�ƫ�ƣ�dxAngle = ����ƫ�� �� ��Ԫ�ߴ� / ����
//   2. ���Է�λ�� = ��̨ˮƽ�� + ˮƽ�Ƕ�ƫ��
//   3. ʹ�� Haversine ��ʽ�����豸 GPS + ��λ�� + ���� �� Ŀ�� GPS
//
// Haversine ��ʽ:
//   lat2 = asin(sin(lat1)��cos(d/R) + cos(lat1)��sin(d/R)��cos(bearing))
//   lon2 = lon1 + atan2(sin(bearing)��sin(d/R)��cos(lat1), cos(d/R) - sin(lat1)��sin(lat2))
//   ���� R = 6371000m (����ƽ���뾶)
//============================================================================
void MainWindow::pixelToGps(double pixelX, double pixelY, double distance,
                              double& outLat, double& outLon)
{
    CameraConfig& cam = m_cfg->cam();
    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
    double focal = isVis ? cam.visMinFocal * m_currentVisZoom
                         : cam.irMinFocal * m_currentIrZoom;
    int resX = isVis ? cam.visResX : cam.irResX;
    int resY = isVis ? cam.visResY : cam.irResY;
    int halfW = resX / 2, halfH = resY / 2;

    // �� UI �ؼ���ȡ�豸���� GPS ����̨�Ƕȣ��� ZoomInfo ֡���£�
    double devLat = parseCoord(ui->statLatitude->text());
    double devLon = parseCoord(ui->statLongitude->text());
    double pan = ui->statPanAngle->text().toDouble();

    if (devLat == 0 && devLon == 0) { outLat = 0; outLon = 0; return; }
    if (focal < 0.1) { outLat = 0; outLon = 0; return; }

    double dxAngle = (pixelX - halfW) * px / (focal * 1000.0);

    // ���Է�λ�� = ��̨ˮƽ��(�ȡ�����) + ����ˮƽƫ�ƽ�(����)
    double bearing = pan * M_PI / 180.0 + dxAngle;
    double range = distance > 0 ? distance : 100.0; // Ĭ�� 100m

    // Haversine ��ʽ����Ŀ�꾭γ��
    double R = 6371000.0;                          // ����ƽ���뾶 (m)
    double lat1 = devLat * M_PI / 180.0;           // �豸γ�� �� ����
    double lon1 = devLon * M_PI / 180.0;           // �豸���� �� ����
    double d = range / R;                          // �����Ӧ�����Ľ�

    double lat2 = qAsin(qSin(lat1) * qCos(d) + qCos(lat1) * qSin(d) * qCos(bearing));
    double lon2 = lon1 + qAtan2(qSin(bearing) * qSin(d) * qCos(lat1), qCos(d) - qSin(lat1) * qSin(lat2));

    outLat = lat2 * 180.0 / M_PI;  // ���ת�ض�
    outLon = lon2 * 180.0 / M_PI;
}

//============================================================================
// pixelBboxToGps - ���ؿ�ǵ�ת GPS��������У����
// �� pixelToGps ���ƣ������������̨������ (tilt) �����ش�ֱƫ�� (dyAngle)
// �Բ��������У������Ŀ���ڻ�����ƫ������ʱ��ʵ�ʹ�·���벻ͬ��
//
// У��ԭ���
//   H = distance �� sin(tilt)          �� �豸����߶�
//   effTilt = tilt + dyAngle          �� Ŀ�������ˮƽ���ʵ�ʸ�����
//   rangeAdj = H / sin(effTilt)       �� У�����б��
//   �� effTilt �ӽ� 0 �� �� ʱ����У����������㣩
//============================================================================
void MainWindow::pixelBboxToGps(double pixelX, double pixelY, double distance,
                                  double tiltDeg, double& outLat, double& outLon)
{
    CameraConfig& cam = m_cfg->cam();
    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
    double focal = isVis ? cam.visMinFocal * m_currentVisZoom
                         : cam.irMinFocal * m_currentIrZoom;
    int resX = isVis ? cam.visResX : cam.irResX;
    int resY = isVis ? cam.visResY : cam.irResY;
    int halfW = resX / 2, halfH = resY / 2;

    double devLat = parseCoord(ui->statLatitude->text());
    double devLon = parseCoord(ui->statLongitude->text());
    double pan = ui->statPanAngle->text().toDouble();

    if (devLat == 0 && devLon == 0) { outLat = 0; outLon = 0; return; }
    if (focal < 0.1) { outLat = 0; outLon = 0; return; }

    double dxAngle = (pixelX - halfW) * px / (focal * 1000.0);
    double dyAngle = (pixelY - halfH) * px / (focal * 1000.0);

    // ������̨�����������ش�ֱƫ��У�����ֵ
    double tiltRad = tiltDeg * M_PI / 180.0;
    double rangeAdj = distance;
    if (tiltRad > 0.01) {
        double H = distance * qSin(tiltRad);          // �豸��Ը߶�
        double effTilt = tiltRad + dyAngle;           // Ŀ��ʵ�ʸ�����
        if (effTilt > 0.005 && effTilt < M_PI - 0.005)
            rangeAdj = H / qSin(effTilt);             // У��б��
    }

    double bearing = pan * M_PI / 180.0 + dxAngle;
    double range = rangeAdj > 0 ? rangeAdj : (distance > 0 ? distance : 100.0);

    // Haversine ��ʽ����Ŀ�� GPS (ͬ pixelToGps)
    double R = 6371000.0;
    double lat1 = devLat * M_PI / 180.0;
    double lon1 = devLon * M_PI / 180.0;
    double d = range / R;

    double lat2 = qAsin(qSin(lat1) * qCos(d) + qCos(lat1) * qSin(d) * qCos(bearing));
    double lon2 = lon1 + qAtan2(qSin(bearing) * qSin(d) * qCos(lat1), qCos(d) - qSin(lat1) * qSin(lat2));

    outLat = lat2 * 180.0 / M_PI;
    outLon = lon2 * 180.0 / M_PI;
}

// Haversine ��ʽ�����������루�ף�
static double haversineDistance(double lat1, double lon1, double lat2, double lon2)
{
    double R = 6371000.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = qSin(dLat / 2) * qSin(dLat / 2)
             + qCos(lat1 * M_PI / 180.0) * qCos(lat2 * M_PI / 180.0)
             * qSin(dLon / 2) * qSin(dLon / 2);
    double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return R * c;
}

// ���� �켣���ϡ��ֵ ����
static constexpr double TRK_MIN_DIST_M     = 3.0;    // ����������С�ڴ˾���ֱ�Ӷ���
static constexpr double TRK_MAX_DIST_M     = 20.0;   // �������ϡ�������˾���ǿ�ƴ��
static constexpr double TRK_HEADING_DIFF_DEG = 15.0; // �����ƫת��ֵ��������ǿ�ƴ��
static constexpr int    TRK_HEARTBEAT_MS   = 2500;   // ��������������ʱ��ǿ�ƴ��

// bearing - ��������֮��ĺ���ǣ��ȣ�������Ϊ0�㣬˳ʱ��
static double bearing(double lat1, double lon1, double lat2, double lon2)
{
    double lat1R = qDegreesToRadians(lat1);
    double lat2R = qDegreesToRadians(lat2);
    double lon1R = qDegreesToRadians(lon1);
    double lon2R = qDegreesToRadians(lon2);
    double dLon = lon2R - lon1R;
    double y = qSin(dLon) * qCos(lat2R);
    double x = qCos(lat1R) * qSin(lat2R) - qSin(lat1R) * qCos(lat2R) * qCos(dLon);
    double deg = qRadiansToDegrees(qAtan2(y, x));
    return deg < 0 ? deg + 360.0 : deg;
}

// shouldPlotTrackPoint - ��ϡ�ж����Ƿ�Ӧ����ǰGPS����Ƶ���ͼ
// ���� < TRK_MIN_DIST_M  �� ������������
// ���� > TRK_MAX_DIST_M  �� ���㣨�������ϡ��
// �����ƫת > TRK_HEADING_DIFF_DEG �� ���㣨ת�������
// ���ϴλ��� > TRK_HEARTBEAT_MS    �� ���㣨�������
// outBearing����ѡ��: ���ؼ�����ĺ���ǣ�������ô��ظ����� bearing()
static bool shouldPlotTrackPoint(double newLat, double newLon,
                                  double plotLat, double plotLon,
                                  double plotHeading, const QDateTime& plotTime,
                                  double* outBearing = nullptr)
{
    if (plotHeading < 0) return true; // �״λ��� / Ŀ���л�

    double dist = haversineDistance(plotLat, plotLon, newLat, newLon);

    // ����������Ư��ֱ�Ӷ���
    if (dist < TRK_MIN_DIST_M) return false;

    // �������ϡ
    if (dist > TRK_MAX_DIST_M) {
        if (outBearing) *outBearing = bearing(plotLat, plotLon, newLat, newLon);
        return true;
    }

    // ����Ǳ仯
    double head = bearing(plotLat, plotLon, newLat, newLon);
    double diff = qAbs(head - plotHeading);
    if (diff > 180.0) diff = 360.0 - diff;
    if (diff >= TRK_HEADING_DIFF_DEG) {
        if (outBearing) *outBearing = head;
        return true;
    }

    // �������ף����ں���֮�󣬱�������ת��ʱ�����������㣩
    if (plotTime.isValid()) {
        qint64 elapsed = plotTime.msecsTo(QDateTime::currentDateTime());
        if (elapsed >= TRK_HEARTBEAT_MS) {
            if (outBearing) *outBearing = head;
            return true;
        }
    }

    return false;
}

//============================================================================
// updateMapTargets - ���µ�ͼ�ϵ� AI Ŀ����
// ����ģʽ (WorkMode 2~4)��
//   - ����ʾ 1 ��Ŀ�꣨���� 0xB1 ���ȣ���ʧ 0xB2 ��֮��
//   - �״�ʧ����0xB2��ʱ��¼ʱ�䣬�������λ�� 5 ��
//   - ʧ���� 5 ������켣��Ŀ���
// ʶ��ģʽ (WorkMode 1)����ʾȫ��ʶ��Ŀ�꣬���ϱ�ʱ�������
//============================================================================
void MainWindow::updateMapTargets(const QJsonObject& doc, int workMode)
{
    bool hasObject = doc.contains("Object") && doc.value("Object").isObject();
    QJsonObject objMap;
    if (hasObject) objMap = doc.value("Object").toObject();
    double tilt = m_currentTilt;

    //==========================================================================
    // ʶ��ģʽ (WorkMode=1)����ʾ����Ŀ�꣬���ϱ�ʱ���
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
                pixelToGps(cx, cy, dist, tLat, tLon);

                QJsonArray bbox;
                double bLat, bLon;
                pixelBboxToGps(L, T, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                pixelBboxToGps(R, T, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                pixelBboxToGps(R, B, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                pixelBboxToGps(L, B, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});

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
    // ����ģʽ (WorkMode=2~4)
    //==========================================================================
    if (workMode >= 2 && workMode <= 4) {

        // -- ��������Ŀ�� 0xB1 �Ͷ�ʧĿ�� 0xB2 --
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

        // ---- ������Ŀ�� ----
        if (!lockedId.isEmpty()) {
            m_track.lostSince = QDateTime();

            int cls = lockedObj.value("Class").toInt();
            double tLat = 0, tLon = 0;

            double dist = calcVisualDistance(lockedObj, cls, true);
            // ���� AI Ŀ����루���� ZoomInfo �޼�����ʱ���ˣ�
            m_lastAiDist = dist;
            m_lastAiDistEstimated = (lockedObj.value("Distance").toDouble(0) <= 0 && dist > 0);

            if (lockedObj.contains("Points")) {
                QJsonObject pts = lockedObj.value("Points").toObject();
                int L = pts.value("Left").toInt(), T = pts.value("Top").toInt();
                int R = pts.value("Right").toInt(), B = pts.value("Bottom").toInt();
                double cx = (L + R) / 2.0, cy = (T + B) / 2.0;
                pixelToGps(cx, cy, dist, tLat, tLon);

                m_track.lat = tLat;
                m_track.lon = tLon;
                m_track.cls = cls;

                // �����ٶȣ���/�룩
                double speed = 0;
                if (m_track.prevTime.isValid()) {
                    double dist_m = haversineDistance(m_track.prevLat, m_track.prevLon, tLat, tLon);
                    double dt_s = m_track.prevTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
                    if (dt_s > 0) speed = dist_m / dt_s;
                }
                m_track.prevLat = tLat;
                m_track.prevLon = tLon;
                m_track.prevTime = QDateTime::currentDateTime();

                // �켣���ϡ�ж�
                if (tLat != 0 && tLon != 0) {
                    // Ŀ���л�ʱ���ó�ϡ״̬��ȷ����Ŀ���׵�ض�����
                    bool targetChanged = (m_track.id != lockedId);
                    m_track.id = lockedId;
                    if (targetChanged)
                        m_track.plotHeading = -1;

                    double outBearing = 0;
                    if (shouldPlotTrackPoint(tLat, tLon,
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
                pixelBboxToGps(L, T, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                pixelBboxToGps(R, T, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                pixelBboxToGps(R, B, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                pixelBboxToGps(L, B, dist, tilt, bLat, bLon); bbox.append(QJsonArray{bLat, bLon});
                t[QStringLiteral("bbox")] = bbox;
                targetArr.append(t);
                m_mapWidget->updateTargetMarkers(targetArr);
            }
            return;
        }

        // ---- �ж�ʧĿ�� (0xB2) ----
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
                pixelToGps(cx, cy, dist, tLat, tLon);
                hasPts = true;
            }

            // �״�ʧ��������λ�ã���¼ʱ��
            if (m_track.lostSince.isNull()) {
                m_track.id = lostId;
                m_track.lat = tLat;
                m_track.lon = tLon;
                m_track.cls = cls;
                m_track.lostSince = QDateTime::currentDateTime();
            }

            // ����Ƿ񳬹� 5 ��
            qint64 elapsed = m_track.lostSince.msecsTo(QDateTime::currentDateTime());
            if (elapsed >= 5000) {
                // ���� 5 �룬����켣��Ŀ��
                m_mapWidget->clearAllTracks();
                m_mapWidget->updateTargetMarkers(QJsonArray());
                return;
            }

            // 5 ���ڣ���ʾ���λ��
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

        // ---- �� Object ���� 0xB1/0xB2����� ----
        m_mapWidget->clearAllTracks();
        m_mapWidget->updateTargetMarkers(QJsonArray());
    }
}

// calcVisualDistance - ��װ��"�޼�����ʱ���Ӿ����������"�Ĺ����߼�
// obj: Ŀ�� JSON �������� Distance �ֶκ� Points �ֶΣ�
// cls: Ŀ�� Class ����
// updateTrackLabel: �Ƿ���� trackDistance ״̬���ı�����������/��ʧʱ true��
// ����ֵ�����м�������򷵻�ԭֵ�����򷵻ع���ֵ
double MainWindow::calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel)
{
    double dist = obj.value("Distance").toDouble(0);
    if (dist > 0 || !obj.contains("Points"))
        return dist;

    int low = currentAlgoModel() % 10;
    double ref = m_cfg->cam().targetRefSize(low, cls);

    // ����״̬ (0xB1/0xB2) �޷�ͨ�� Class �鵽�ο��ߴ�
    // �� ���㷨ģ�ͱ�����֪ Class ���Ӿ�����
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

    dist = estimateTargetDistance(boxPx, focal, pxSize, ref);
    if (updateTrackLabel)
        ui->trackDistance->setText(QString::number(dist, 'f', 1) + QStringLiteral(" m (����)"));
    return dist;
}

// �Ӿ���������㣺��֪Ŀ��ο��ߴ磬�����ش�С���ƾ���
// ��ʽ������(m) = �ο��ߴ�(m) �� ����(mm) �� 1000 / (Ŀ�������� �� ��Ԫ�ߴ�(��m))
double MainWindow::estimateTargetDistance(int boxPixels, double focalMm, double pixelSizeUm, double refSize)
{
    if (boxPixels <= 0 || focalMm < 0.1 || pixelSizeUm <= 0 || refSize <= 0)
        return 0.0;
    return qBound(1.0, refSize * focalMm * 1000.0 / (boxPixels * pixelSizeUm), 10000.0);
}

//============================================================================
// resizeEvent - ��������ʱ���²���
//============================================================================
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateMapLayout();
}

//============================================================================
// toggleMap - �л���ͼ��ʾ/����
// �� m_btnMap ����
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
        if (ui->videoWidget->parent() != ui->widgetDisplay) {
            ui->videoWidget->setParent(ui->widgetDisplay);
            ui->verticalLayout_display->addWidget(ui->videoWidget);
        }
        m_mapContainer->setVisible(false);
    }
}

//============================================================================
// toggleMapMode - �л�����/ȫ��ģʽ
// ����ģʽ�µ�����ͼ����չ����ȫ��ģʽ�µ� ? �ջ�
//============================================================================
void MainWindow::toggleMapMode()
{
    m_mapExpanded = !m_mapExpanded;
    updateMapLayout();
}

//============================================================================
// updateMapLayout - ��ģʽ����
//   ��ͼ����    �� videoWidget ���� widgetDisplay
//   ����ģʽ    �� videoWidget ȫ�� + 280 Բ�θ���
//   ȫ��/���ͼ  �� mapWidget ���� widgetDisplay + ���� PiP �Ի���
//============================================================================
void MainWindow::updateMapLayout()
{
    QSize ps = ui->widgetDisplay->size();
    if (ps.isEmpty()) return;

    if (!m_mapVisible) {
        if (ui->videoWidget->parent() != ui->widgetDisplay) {
            ui->videoWidget->setParent(ui->widgetDisplay);
            ui->verticalLayout_display->addWidget(ui->videoWidget);
            ui->videoWidget->setVisible(true);
        }
        m_mapContainer->setVisible(false);
        m_pipDialog->hide();
        return;
    }

    m_mapContainer->setVisible(true);

    if (m_mapExpanded) {
        // ���ͼ����ͼ������ʾ��
        m_mapContainer->setGeometry(0, 0, ps.width(), ps.height());
        m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, false);
        m_mapContainer->clearMask();
        m_mapWidget->setGeometry(0, 0, ps.width(), ps.height());
        m_mapWidget->setCircularClip(false);
        m_mapOverlay->setVisible(false);

        // ��Ƶ�������� PiP �Ի���
        ui->videoWidget->setParent(m_pipDialog);
        m_pipDialog->layout()->addWidget(ui->videoWidget);
        m_pipPos = QPoint(8, ps.height() - 240 - 8);
        m_pipDialog->move(m_pipPos);
        m_pipDialog->show();
        ui->videoWidget->setVisible(true);
    } else {
        // ����ģʽ
        if (ui->videoWidget->parent() != ui->widgetDisplay) {
            ui->videoWidget->setParent(ui->widgetDisplay);
            ui->verticalLayout_display->addWidget(ui->videoWidget);
            ui->videoWidget->setVisible(true);
        }
        m_pipDialog->hide();

        m_mapContainer->setGeometry(m_miniMapPos.x(), m_miniMapPos.y(), 280, 280);
        m_mapContainer->setAttribute(Qt::WA_TranslucentBackground, true);
        m_mapContainer->setMask(QRegion(0, 0, 280, 280, QRegion::Ellipse));
        m_mapWidget->setGeometry(0, 0, 280, 280);
        double lat = parseCoord(ui->statLatitude->text());
        double lon = parseCoord(ui->statLongitude->text());
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
// eventFilter - ȫ���¼�����
//   QComboBox�����ع��֣��������б�չ��ʱ����������л�
//   m_mapOverlay��������չ������ק���ƶ�λ��
//   m_pipTitle����ק�ƶ� PiP �Ի���λ��
//============================================================================
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // QComboBox �������أ�δչ��ʱ���Թ����¼�
    if (event->type() == QEvent::Wheel) {
        auto *cb = qobject_cast<QComboBox*>(obj);
        if (cb && !cb->view()->isVisible()) {
            return true;  // �̵�����¼�
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
            // ˫�����������ָ���Ƶ������ʾ�� + ��ʾ�����ͼ
            toggleMapMode();
            return true;
        }
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

//============================================================================
// onAiCleanupTimeout - AIInfo ��ʱ����
// �豸��Ŀ��ʱ���� AIInfo ֡������ 2 ���޸�������������������
//============================================================================
void MainWindow::onAiCleanupTimeout()
{
    if (!m_lastAiInfoTime.isValid()) return;
    if (m_lastAiInfoTime.msecsTo(QDateTime::currentDateTime()) < 2000) return;

    m_lastAiInfoTime = QDateTime();

    // ���ʶ����
    ui->lblIdentifyCount->setText(QString::fromUtf8("Ŀ������: 0"));
    ui->tableIdentify->setRowCount(0);

    // ��ո������
    ui->lblTrackStatus->setText(QString::fromUtf8("״̬: δ����"));
    ui->lblTrackStatus->setProperty("state", "nolock");
    refreshStyle(ui->lblTrackStatus);
    ui->trackPos->clear();
    ui->trackMissDistance->clear();
    ui->trackDistance->clear();

    // ��յ�ͼ��Ǻ͹켣
    m_mapWidget->clearAllTracks();
    m_mapWidget->updateTargetMarkers(QJsonArray());
    m_mapWidget->clearFov();

    // ���ø���״̬
    m_track = TrackState();
    m_lastAiDist = 0;
    m_lastAiDistEstimated = false;

    qDebug() << "[AIInfo] Cleanup triggered: no AIInfo for 2s";
}

int MainWindow::currentAlgoModel() const
{
    return m_currentAlgoModel;
}




