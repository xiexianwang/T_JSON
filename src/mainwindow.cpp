//============================================================================
// mainwindow.cpp - T-JSON 主窗口实现
// 包含主窗口的构造/析构、UI 样式初始化、信号-槽连接，
// 以及完整的 JSON 帧解析、状态更新、地图坐标转换、云台镜头控制逻辑。
//============================================================================
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "rtspthread.h"
#include "videowidget.h"
#include "mapdialog.h"
#include "mapwidget.h"
#include "cmdlogdialog.h"
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QtMath>

//============================================================================
// 构造函数：初始化所有子模块、建立信号-槽连接、配置 UI
//============================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(new TJsonClient(this))        // TCP JSON 协议客户端
    , m_cfg(new ConfigManager(this))         // 持久化配置管理
    , m_device(new DeviceController(m_client, m_cfg, this))  // 设备指令控制器
    , m_rtsp(new RtspThread(this))           // RTSP 视频拉流线程
    , m_updatingFromDevice(false)            // 防递归更新初始关闭
    , m_currentVisZoom(1.0)                  // 默认可见光倍率 1.0
    , m_currentIrZoom(1.0)                   // 默认红外倍率 1.0
    , m_currentPipShow(0)                    // 默认显示模式：大图可见光
{
    ui->setupUi(this);
    setupUiStyles();

    //============================================================================
    // 地图按钮：插入到顶部工具栏"断开视频"按钮之后
    // 点击弹出 MapDialog，显示设备 GPS 位置、视场角扇形、目标标记等
    //============================================================================
    m_btnMap = new QPushButton(QStringLiteral("🗺 地图"), this);
    m_btnMap->setFixedSize(66, 26);
    m_btnMap->setStyleSheet(
        "QPushButton { background:#2d5a88; color:white; border:1px solid #3a7abf; border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:#3a7abf; }");

    auto *top2 = findChild<QHBoxLayout*>(QStringLiteral("horizontalLayout_top2"));
    if (top2) {
        int idx = top2->indexOf(ui->btnVideoDisconnect);
        top2->insertWidget(idx + 1, m_btnMap);
    }

    m_mapDlg = new MapDialog(this);
    connect(m_btnMap, &QPushButton::clicked, this, [this]() {
        m_mapDlg->show();
        m_mapDlg->raise();
        m_mapDlg->activateWindow();
    });

    //============================================================================
    // RTSP 视频流信号连接
    // RtspThread 在工作线程中拉流解码，通过信号将帧数据传回主线程
    // VideoWidget 的 selectionFinished 信号用于框选跟踪
    //============================================================================
    connect(m_rtsp, &RtspThread::frameReady, this, &MainWindow::onRtspFrame);
    connect(m_rtsp, &RtspThread::streamOpened, this, &MainWindow::onRtspOpened);
    connect(m_rtsp, &RtspThread::streamError, this, &MainWindow::onRtspError);
    connect(ui->videoWidget, &VideoWidget::selectionFinished, this, &MainWindow::onVideoSelection);

    //============================================================================
    // T-JSON 协议信号连接
    // TJsonClient 管理 TCP 长连接、心跳保活、JSON 帧收发与自动重连
    //============================================================================
    connect(m_client, &TJsonClient::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(m_client, &TJsonClient::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);
    connect(m_client, &TJsonClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_client, &TJsonClient::jsonReceived, this, &MainWindow::onJsonReceived);
    connect(m_client, &TJsonClient::imageSnapped, this, &MainWindow::onImageSnapped);
    connect(m_client, &TJsonClient::ackReceived, this, &MainWindow::onAckReceived);
    
    // 自动重连信号：每次重连尝试时更新按钮文本与状态栏提示
    connect(m_client, &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        Q_UNUSED(maxRetries);
        ui->btnConnect->setText(QString::fromUtf8("重连中(次数:%1)").arg(attempt));
        ui->btnConnect->setStyleSheet("background-color: #d68b00;");
        ui->statusbar->showMessage(QString::fromUtf8("网络波动，正在进行第 %1 次自动探测重连...").arg(attempt));
    });
    // 重连失败：恢复按钮初始状态
    connect(m_client, &TJsonClient::reconnectFailed, this, [this]() {
        ui->btnConnect->setText(QString::fromUtf8("连接设备"));
        ui->btnConnect->setEnabled(true);
        ui->btnConnect->setStyleSheet("");
        ui->statusbar->showMessage(QString::fromUtf8("重连失败，已放弃连接"), 5000);
    });

    //============================================================================
    // 云台八方向控制 (基于 Pelco-D 协议)
    // 按下按钮 → 发送持续转动指令；释放按钮 → 发送停止指令
    // 八个按钮分别对应 Up/Down/Left/Right 及四个对角线方向
    //============================================================================
    auto connectPtzBtn = [this](QPushButton* btn, PtzDir dir) {
        connect(btn, &QPushButton::pressed, this, [this, dir]() { m_device->ptzMove(dir); });
        connect(btn, &QPushButton::released, this, [this]() { m_device->ptzStop(); });
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
    // 当前镜头目标(comboLensTarget)决定操作可见光还是红外镜头
    //============================================================================
    auto connectLensBtn = [this](QPushButton* btn, int op) {
        connect(btn, &QPushButton::pressed, this, [this, op]() {
            int t = ui->comboLensTarget->currentIndex();
            if (op == 0) m_device->lensZoomIn(t);
            else if (op == 1) m_device->lensZoomOut(t);
            else if (op == 2) m_device->lensFocusIn(t);
            else m_device->lensFocusOut(t);
        });
        connect(btn, &QPushButton::released, this, [this]() { m_device->lensStop(); });
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
    ui->sliderFocusSpeed->setValue(m_cfg->lens().focusSpeed);
    ui->spinFocusSpeed->setValue(m_cfg->lens().focusSpeed);

    connect(ui->sliderZoomSpeed, &QSlider::valueChanged, ui->spinZoomSpeed, &QSpinBox::setValue);
    connect(ui->spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderZoomSpeed, &QSlider::setValue);
    connect(ui->spinZoomSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->lens().zoomSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    connect(ui->sliderFocusSpeed, &QSlider::valueChanged, ui->spinFocusSpeed, &QSpinBox::setValue);
    connect(ui->spinFocusSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderFocusSpeed, &QSlider::setValue);
    connect(ui->spinFocusSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->lens().focusSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    //============================================================================
    // 预置位控制 (调用/设置/删除)
    // 通过 spinPreset 选择预置位编号，调用 DeviceController 中的协议封装
    //============================================================================
    connect(ui->btnCallPreset, &QPushButton::clicked, this, [this]() {
        m_device->callPreset(ui->spinPreset->value());
    });
    connect(ui->btnSetPreset, &QPushButton::clicked, this, [this]() {
        m_device->setPreset(ui->spinPreset->value());
    });
    connect(ui->btnDelPreset, &QPushButton::clicked, this, [this]() {
        m_device->delPreset(ui->spinPreset->value());
    });

    //============================================================================
    // 附加功能开关 (数字变倍 / 自动变倍 / 抓拍上传 / 位置归零)
    // 每个 CheckBox 直连对应的设备指令
    //============================================================================
    connect(ui->checkDigitalZoom, &QCheckBox::toggled, this, [this](bool checked) {
        m_device->setDigitalZoom(checked);
    });
    connect(ui->checkAutoZoom, &QCheckBox::toggled, this, [this](bool checked) {
        m_device->setAutoZoom(checked);
    });
    connect(ui->checkCaptureUpload, &QCheckBox::toggled, this, [this](bool checked) {
        m_device->setCaptureUpload(checked);
    });
    connect(ui->checkPosReset, &QCheckBox::toggled, this, [this](bool checked) {
        m_device->posReset(checked);
    });

    //============================================================================
    // 指令日志窗口
    // 实时显示所有下发给设备的指令内容，方便调试与协议分析
    //============================================================================
    auto *cmdLog = new CmdLogDialog(this);
    connect(m_device, &DeviceController::commandSent, cmdLog, &CmdLogDialog::appendLog);
    cmdLog->show();
}

//============================================================================
// 析构函数：释放 UI 资源
// 子模块对象 (m_client, m_cfg, m_device, m_rtsp, m_mapDlg) 
// 均以 MainWindow 为父对象，由 Qt 对象树自动析构
//============================================================================
MainWindow::~MainWindow()
{
    delete ui;
}

//============================================================================
// setupUiStyles - 初始化 UI 样式（QSS 已取消加载）
//============================================================================
void MainWindow::setupUiStyles()
{
    ui->tableIdentify->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

//============================================================================
// on_btnConnect_clicked - 连接/断开设备按钮
// 已连接时点击为断开；未连接时读取 IP 和端口发起 TCP 连接
//============================================================================
void MainWindow::on_btnConnect_clicked()
{
    if (m_client->isConnected()) {
        m_client->disconnectDevice();
    } else {
        QString ip = ui->lineEditIp->text();
        quint16 port = ui->spinBoxPort->value();
        m_client->connectToDevice(ip, port);
        ui->btnConnect->setText(QString::fromUtf8("连接中..."));
        ui->btnConnect->setEnabled(false);
        ui->btnCancelConnect->setVisible(true);
    }
}

//============================================================================
// on_btnCancelConnect_clicked - 取消正在进行的连接
// 直接断开 TCP 连接并恢复按钮状态
//============================================================================
void MainWindow::on_btnCancelConnect_clicked()
{
    m_client->disconnectDevice();
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("已取消连接"), 3000);
}

//============================================================================
// on_btnVideoConnect_clicked - 连接 RTSP 视频流
// 从输入框获取 RTSP URL 后交给 RtspThread 进行拉流
//============================================================================
void MainWindow::on_btnVideoConnect_clicked()
{
    QString url = ui->lineEditRtsp->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "RTSP", "请输入 RTSP 地址");
        return;
    }
    m_rtsp->openStream(url);
    ui->btnVideoConnect->setEnabled(false);
    ui->btnVideoConnect->setText(QString::fromUtf8("连接中..."));
    ui->statusbar->showMessage(QString::fromUtf8("正在连接 RTSP 视频流..."));
}

//============================================================================
// on_btnVideoDisconnect_clicked - 断开 RTSP 视频流
// 停止拉流线程、清除视频画面、恢复按钮状态
//============================================================================
void MainWindow::on_btnVideoDisconnect_clicked()
{
    m_rtsp->closeStream();
    ui->videoWidget->clearFrame();
    ui->btnVideoConnect->setEnabled(true);
    ui->btnVideoConnect->setText(QString::fromUtf8("连视频"));
    ui->statusbar->showMessage(QString::fromUtf8("视频已断开"), 3000);
}

//============================================================================
// onRtspFrame - 收到一帧 RTSP 视频图像
// 将解码后的 QImage 传递给 VideoWidget 进行渲染
//============================================================================
void MainWindow::onRtspFrame(const QImage &frame)
{
    ui->videoWidget->setFrame(frame);
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
    ui->videoWidget->clearFrame();
    ui->btnVideoConnect->setEnabled(true);
    ui->btnVideoConnect->setText(QString::fromUtf8("连视频"));
    ui->statusbar->showMessage(msg);
}

//============================================================================
// onVideoSelection - 用户在视频画面上的框选操作
// 将框选的像素坐标与宽高发送给设备，用于框选跟踪模式
// cx, cy 为框选区域中心像素坐标，pw, ph 为框宽高
//============================================================================
void MainWindow::onVideoSelection(int cx, int cy, int pw, int ph)
{
    ui->statusbar->showMessage(
        QString::fromUtf8("框选跟踪: 像素中心(%1,%2) 宽%3高%4")
            .arg(cx).arg(cy).arg(pw).arg(ph));

    m_device->setBoxTrack(cx, cy, pw, ph);
}

//============================================================================
// onDeviceConnected - 设备连接成功回调
// 更新按钮样式为红色"断开连接"，自动查询设备当前图像参数
//============================================================================
void MainWindow::onDeviceConnected()
{
    ui->btnConnect->setText(QString::fromUtf8("断开连接"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("background-color: #c75450;");
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("已连接到设备"), 3000);

    // 连接成功后自动请求一次图像参数，以便 UI 与设备状态同步
    m_device->queryImageParams();
}

//============================================================================
// onDeviceDisconnected - 设备断开回调
// 恢复连接按钮的初始外观
//============================================================================
void MainWindow::onDeviceDisconnected()
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("");
    ui->btnCancelConnect->setVisible(false);
    ui->statusbar->showMessage(QString::fromUtf8("设备已断开"), 3000);
}

//============================================================================
// onErrorOccurred - 连接错误处理
// 弹出消息框显示具体错误，同时恢复按钮初始状态
//============================================================================
void MainWindow::onErrorOccurred(const QString& errorMsg)
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("");
    ui->btnCancelConnect->setVisible(false);
    QMessageBox::warning(this, QString::fromUtf8("连接错误"), errorMsg);
}

//============================================================================
// onAckReceived - 处理设备返回的 ACK 应答
// ACK 状态码:
//   0 = 成功, 1 = 指令不完整, 2 = 指令内容错误
// 在状态栏短暂显示供操作人员确认
//============================================================================
void MainWindow::onAckReceived(quint8 statusCode)
{
    QString msg;
    switch (statusCode) {
    case 0: msg = QString::fromUtf8("指令执行成功"); break;
    case 1: msg = QString::fromUtf8("指令不完整");   break;
    case 2: msg = QString::fromUtf8("指令内容错误");  break;
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
            // PipShow: 0=大图可见光, 1=红外, 2=可见光, 3=融合, 4=大图红外
            bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
            double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                              : cam.irMinFocal * m_currentIrZoom;
            int halfW = (isVis ? cam.visResX : cam.irResX) / 2;
            int halfH = (isVis ? cam.visResY : cam.irResY) / 2;

            // Object 字段是一个字典，key 为目标 ID，value 为目标属性
            if (doc.contains("Object") && doc.value("Object").isObject()) {
                QJsonObject objMap = doc.value("Object").toObject();
                for (auto it = objMap.begin(); it != objMap.end(); ++it) {
                    QString id = it.key();
                    QJsonObject obj = it.value().toObject();

                    int r = ui->tableIdentify->rowCount();
                    ui->tableIdentify->insertRow(r);
                    ui->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
                    ui->tableIdentify->setItem(r, 1, new QTableWidgetItem(QString::number(obj.value("Class").toInt())));
                    ui->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(obj.value("Distance").toDouble(), 'f', 1)));

                    if (obj.contains("Points")) {
                        QJsonObject pts = obj.value("Points").toObject();
                        int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                        int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                        QString pos = QString("(%1,%2)").arg(l).arg(t);
                        ui->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));

                        // 计算目标中心相对于画面中心的脱靶量（毫弧度）
                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        QString miss = missMradStr(cx - halfW, cy - halfH, px, fl);
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
                ui->lblTrackStatus->setStyleSheet(locked ? "color: #00cc00; font-weight: bold;"
                                                         : "color: #cc0000; font-weight: bold;");

                QString objId = objMap.begin().key();
                ui->trackId->setText(objId);
                ui->trackClass->setText(QString::number(cls));

                if (obj.contains("Distance"))
                    ui->trackDistance->setText(QString::number(obj.value("Distance").toDouble(), 'f', 1));
                else
                    ui->trackDistance->clear();

                if (obj.contains("Angle"))
                    ui->trackAngle->setText(QString::number(obj.value("Angle").toDouble(), 'f', 1));

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    ui->trackPos->setText(QString("(%1,%2)-(%3,%4)").arg(l).arg(t).arg(r2).arg(b));

                    // 计算目标脱靶量：目标中心距画面中心的像素偏差→毫弧度
                    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
                    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
                    double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                                      : cam.irMinFocal * m_currentIrZoom;
                    int halfW = (isVis ? cam.visResX : cam.irResX) / 2;
                    int halfH = (isVis ? cam.visResY : cam.irResY) / 2;
                    double objCx = (l + r2) / 2.0, objCy = (t + b) / 2.0;
                    double dx = objCx - halfW, dy = objCy - halfH;
                    ui->trackMissDistance->setText(missMradStr(dx, dy, px, fl));

                    // 如果设备未提供角度，根据像素偏移自行计算方位角
                    if (!obj.contains("Angle")) {
                        double angleDeg = qAtan2(dy, dx) * 180.0 / 3.14159265358979323846;
                        ui->trackAngle->setText(QString::number(angleDeg, 'f', 1));
                    }
                } else {
                    ui->trackPos->clear();
                    ui->trackMissDistance->clear();
                }
            } else {
                // 无目标：显示"未锁定"并清空所有跟踪字段
                ui->lblTrackStatus->setText(QString::fromUtf8("状态: 未锁定"));
                ui->lblTrackStatus->setStyleSheet("color: #aaaaaa; font-weight: bold;");
                ui->trackId->clear();
                ui->trackClass->clear();
                ui->trackPos->clear();
                ui->trackMissDistance->clear();
                ui->trackDistance->clear();
                ui->trackAngle->clear();
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
        ui->statHeight->setText(QString::number(doc.value("Height").toDouble(), 'f', 1));
        ui->statDistance->setText(QString::number(doc.value("LaserRange").toDouble(), 'f', 1));
        ui->statPanAngle->setText(QString::number(doc.value("PTZInfoH").toDouble(), 'f', 1));
        ui->statTiltAngle->setText(QString::number(doc.value("PTZInfoV").toDouble(), 'f', 1));

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

        // 显示类型映射表 (PIP = Picture-in-Picture)
        static const char* pipMap[] = {"大图可见光", "红外", "可见光", "融合", "大图红外"};
        int pip = doc.value("PipShow").toInt();
        ui->paramPipShow->setText(pip >= 0 && pip < 5 ? QString::fromUtf8(pipMap[pip]) : QString::number(pip));

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

        ui->paramMaxVisFL->setText(doc.value("MaxVisFL").toString());
        ui->paramMaxIRFL->setText(doc.value("MaxIRFL").toString());

        m_currentPipShow = doc.value("PipShow").toInt();

        // 同步 UI 下拉框到设备当前值，同时抑制信号递归
        m_updatingFromDevice = true;
        if (model >= 0 && model < ui->comboAlgoModel->count())
            ui->comboAlgoModel->setCurrentIndex(model);
        int pipShow = doc.value("PipShow").toInt();
        if (pipShow >= 0 && pipShow < ui->comboDisplayMode->count())
            ui->comboDisplayMode->setCurrentIndex(pipShow);
        syncLensTargetByDisplayMode(pipShow);
        m_updatingFromDevice = false;
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

    ui->statZoomVis->setText(QString::number(m_currentVisZoom, 'f', 2));
    ui->statFocalVis->setText(QString::number(visFocal, 'f', 2));
    ui->statFocusVis->clear();

    // HFOV = 2 * atan( sensor_width_mm / (2 * focal_mm) )
    double visHfov = 2.0 * qAtan((cam.visPixelSize * cam.visResX / 1000.0) / (2.0 * visFocal));
    ui->statFovVis->setText(QString::number(visHfov * kRad2Deg, 'f', 2));

    ui->statZoomIR->setText(QString::number(m_currentIrZoom, 'f', 2));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 2));
    ui->statFocusIR->clear();

    double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));
    ui->statFovIR->setText(QString::number(irHfov * kRad2Deg, 'f', 2));
}

//============================================================================
// missMradStr - 计算脱靶量（毫弧度）
// 给定像素偏移 (dx, dy)、像元尺寸 (μm) 和焦距 (mm)，
// 脱靶量 (mrad) = 像素偏移 × 像元尺寸 / 焦距
// 返回总脱靶量的欧几里得距离（标量），保留两位小数
//============================================================================
QString MainWindow::missMradStr(double dx, double dy, double pixelSizeUm, double focalMm)
{
    if (focalMm < 0.1) return QString();
    double dxMrad = dx * pixelSizeUm / focalMm;
    double dyMrad = dy * pixelSizeUm / focalMm;
    double total = qSqrt(dxMrad * dxMrad + dyMrad * dyMrad);
    return QString::number(total, 'f', 2);
}

//============================================================================
// on_radioModeOff_clicked / on_radioModeIdentify_clicked / on_radioModeAutoTrack_clicked
// 工作模式单选按钮：直接调用 DeviceController 切换设备工作模式
//   0 = 关闭 AI, 1 = 识别, 2 = 自动跟踪
// 点选跟踪(3)和框选跟踪(4)由视频框选操作触发
//============================================================================
void MainWindow::on_radioModeOff_clicked() { m_device->setWorkMode(0); }
void MainWindow::on_radioModeIdentify_clicked() { m_device->setWorkMode(1); }
void MainWindow::on_radioModeAutoTrack_clicked() { m_device->setWorkMode(2); }

//============================================================================
// on_btnPtzMoveTo_clicked - 云台转到指定角度 (预留功能，暂未实现)
//============================================================================
void MainWindow::on_btnPtzMoveTo_clicked()
{
}

//============================================================================
// on_btnSettings_clicked - 打开设置对话框
// 模态对话框用于编辑相机参数 (像元尺寸、分辨率、焦距等)，
// 关闭后重载配置以应用更改
//============================================================================
void MainWindow::on_btnSettings_clicked()
{
    SettingsDialog dlg(m_cfg, this);
    dlg.exec();
}

//============================================================================
// on_comboAlgoModel_currentIndexChanged - 算法模型下拉框切换
// 受 m_updatingFromDevice 保护，避免设备回传时重复下发指令
//============================================================================
void MainWindow::on_comboAlgoModel_currentIndexChanged(int index)
{
    if (m_updatingFromDevice) return;
    m_device->setAlgoModel(index);
}

//============================================================================
// on_comboDisplayMode_currentIndexChanged - 显示模式下拉框切换
// 切换时同步镜头目标选择（红外模式自动选中红外镜头），
// 然后下发显示模式变更指令
//============================================================================
void MainWindow::on_comboDisplayMode_currentIndexChanged(int index)
{
    if (m_updatingFromDevice) return;
    syncLensTargetByDisplayMode(index);
    m_device->setDisplayMode(index);
}

//============================================================================
// on_comboLensTarget_currentIndexChanged - 镜头目标切换 (预留)
// 当前该字段仅用于标识，实际镜头控制由按钮事件读取 currentIndex 决定
//============================================================================
void MainWindow::on_comboLensTarget_currentIndexChanged(int index)
{
    Q_UNUSED(index);
}

//============================================================================
// on_btnSetLocation_clicked - 手动设置设备经纬度
// 从输入框读取经纬度字符串，直接下发给设备覆写 GPS 信息
//============================================================================
void MainWindow::on_btnSetLocation_clicked()
{
    QString lat = ui->editSetLat->text().trimmed();
    QString lon = ui->editSetLon->text().trimmed();

    if (lat.isEmpty() || lon.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("参数错误"),
                             QString::fromUtf8("请填写完整的经纬度参数"));
        return;
    }

    m_device->setLocation(lat, lon);
    ui->statusbar->showMessage(QString::fromUtf8("已下发经纬度"), 3000);
}

//============================================================================
// on_btnGetImageParams_clicked - 查询设备当前图像参数
// 设备会以 ImageSetting 类型帧回复，触发 updateStatusFromJson 更新 UI
//============================================================================
void MainWindow::on_btnGetImageParams_clicked()
{
    m_device->queryImageParams();
    ui->statusbar->showMessage(QString::fromUtf8("已发送参数查询请求"), 3000);
}

//============================================================================
// parseCoord - 坐标字符串解析工具函数
// 处理带后缀的经纬度格式，例如 "39.9042N" → 39.9042, "116.4074E" → 116.4074
// 南纬(S)或西经(W)返回负值；若无后缀则直接返回数值
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
    double lat = parseCoord(latStr);
    double lon = parseCoord(lonStr);
    double alt = doc.value("Height").toDouble(0);
    double pan = doc.value("PTZInfoH").toDouble(0);
    double tilt = doc.value("PTZInfoV").toDouble(0);
    double range = doc.value("LaserRange").toDouble(0);
    if (range <= 0) range = 4000;

    qDebug() << "[MapPos] raw:" << latStr << lonStr << "parsed:" << lat << lon;
    if (lat == 0 && lon == 0) return;

    m_mapDlg->mapWidget()->setDevicePosition(lat, lon);

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
    m_mapDlg->mapWidget()->setVisFov(lat, lon, pan, tilt, visHfov, visVfov, 4000);
    m_mapDlg->mapWidget()->setIrFov(lat, lon, pan, tilt, irHfov, irVfov, 2000);
    m_mapDlg->mapWidget()->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range);
}

//============================================================================
// pixelToGps - 像素坐标转 GPS 地理坐标
// 将图像中某像素点 (pixelX, pixelY) 映射到真实地理经纬度。
// 核心步骤：
//   1. 像素偏移 → 角度偏移：dxAngle = 像素偏移 × 像元尺寸 / 焦距
//   2. 绝对方位角 = 云台水平角 + 水平角度偏移
//   3. 使用 Haversine 公式，从设备 GPS + 方位角 + 距离 → 目标 GPS
//
// Haversine 公式:
//   lat2 = asin(sin(lat1)×cos(d/R) + cos(lat1)×sin(d/R)×cos(bearing))
//   lon2 = lon1 + atan2(sin(bearing)×sin(d/R)×cos(lat1), cos(d/R) - sin(lat1)×sin(lat2))
//   其中 R = 6371000m (地球平均半径)
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

    // 从 UI 控件读取设备最新 GPS 与云台角度（由 ZoomInfo 帧更新）
    double devLat = parseCoord(ui->statLatitude->text());
    double devLon = parseCoord(ui->statLongitude->text());
    double pan = ui->statPanAngle->text().toDouble();
    double tilt = ui->statTiltAngle->text().toDouble();

    if (devLat == 0 && devLon == 0) { outLat = 0; outLon = 0; return; }
    if (focal < 0.1) { outLat = 0; outLon = 0; return; }

    // 像素偏移量 → 空间角度偏移量 (单位: 弧度)
    // 公式: angle_rad = pixel_offset × pixel_size_um / (focal_mm × 1000)
    double dxAngle = (pixelX - halfW) * px / (focal * 1000.0);
    double dyAngle = (pixelY - halfH) * px / (focal * 1000.0);

    // 绝对方位角 = 云台水平角(度→弧度) + 像素水平偏移角(弧度)
    double bearing = pan * M_PI / 180.0 + dxAngle;
    double range = distance > 0 ? distance : 100.0; // 默认 100m

    // Haversine 公式计算目标经纬度
    double R = 6371000.0;                          // 地球平均半径 (m)
    double lat1 = devLat * M_PI / 180.0;           // 设备纬度 → 弧度
    double lon1 = devLon * M_PI / 180.0;           // 设备经度 → 弧度
    double d = range / R;                          // 距离对应的球心角

    double lat2 = qAsin(qSin(lat1) * qCos(d) + qCos(lat1) * qSin(d) * qCos(bearing));
    double lon2 = lon1 + qAtan2(qSin(bearing) * qSin(d) * qCos(lat1), qCos(d) - qSin(lat1) * qSin(lat2));

    outLat = lat2 * 180.0 / M_PI;  // 结果转回度
    outLon = lon2 * 180.0 / M_PI;
}

//============================================================================
// pixelBboxToGps - 像素框角点转 GPS（含俯仰校正）
// 与 pixelToGps 类似，但额外根据云台俯仰角 (tilt) 和像素垂直偏移 (dyAngle)
// 对测距距离进行校正：当目标在画面中偏离中心时，实际光路距离不同。
//
// 校正原理：
//   H = distance × sin(tilt)          — 设备架设高度
//   effTilt = tilt + dyAngle          — 目标相对于水平面的实际俯仰角
//   rangeAdj = H / sin(effTilt)       — 校正后的斜距
//   当 effTilt 接近 0 或 π 时跳过校正（避免除零）
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

    // 根据云台俯仰角与像素垂直偏移校正测距值
    double tiltRad = tiltDeg * M_PI / 180.0;
    double rangeAdj = distance;
    if (tiltRad > 0.01) {
        double H = distance * qSin(tiltRad);          // 设备相对高度
        double effTilt = tiltRad + dyAngle;           // 目标实际俯仰角
        if (effTilt > 0.005 && effTilt < M_PI - 0.005)
            rangeAdj = H / qSin(effTilt);             // 校正斜距
    }

    double bearing = pan * M_PI / 180.0 + dxAngle;
    double range = rangeAdj > 0 ? rangeAdj : (distance > 0 ? distance : 100.0);

    // Haversine 公式计算目标 GPS (同 pixelToGps)
    double R = 6371000.0;
    double lat1 = devLat * M_PI / 180.0;
    double lon1 = devLon * M_PI / 180.0;
    double d = range / R;

    double lat2 = qAsin(qSin(lat1) * qCos(d) + qCos(lat1) * qSin(d) * qCos(bearing));
    double lon2 = lon1 + qAtan2(qSin(bearing) * qSin(d) * qCos(lat1), qCos(d) - qSin(lat1) * qSin(lat2));

    outLat = lat2 * 180.0 / M_PI;
    outLon = lon2 * 180.0 / M_PI;
}

//============================================================================
// updateMapTargets - 更新地图上的 AI 目标标记
// 遍历 Object 字典，对每个拥有像素框 (Points) 的目标：
//   1. 计算目标中心 GPS (pixelToGps)
//   2. 跟踪模式下追加轨迹点 (appendTrackPoint)
//   3. 计算目标框四角 GPS (pixelBboxToGps，含俯仰校正)
//   4. 打包所有目标信息为 JSON 数组传递到 MapWidget 渲染
// 无目标时清理地图上的轨道与视场角
//============================================================================
void MainWindow::updateMapTargets(const QJsonObject& doc, int workMode)
{
    if (!doc.contains("Object") || !doc.value("Object").isObject()) {
        if (workMode >= 2) {
            m_mapDlg->mapWidget()->clearAllTracks();
            m_mapDlg->mapWidget()->clearFov();
        }
        return;
    }

    QJsonObject objMap = doc.value("Object").toObject();
    QJsonArray targetArr;
    double tilt = ui->statTiltAngle->text().toDouble();

    for (auto it = objMap.begin(); it != objMap.end(); ++it) {
        QString id = it.key();
        QJsonObject obj = it.value().toObject();
        double dist = obj.value("Distance").toDouble(0);
        int cls = obj.value("Class").toInt();

        double tLat = 0, tLon = 0;

        if (obj.contains("Points")) {
            QJsonObject pts = obj.value("Points").toObject();
            int L = pts.value("Left").toInt();
            int T = pts.value("Top").toInt();
            int R = pts.value("Right").toInt();
            int B = pts.value("Bottom").toInt();
            double cx = (L + R) / 2.0;
            double cy = (T + B) / 2.0;
            pixelToGps(cx, cy, dist, tLat, tLon);

            // 跟踪模式下记录目标轨迹（按 ID 区分不同目标）
            if (workMode >= 2 && tLat != 0 && tLon != 0) {
                m_mapDlg->mapWidget()->appendTrackPoint(id, tLat, tLon);
            }

            // 计算目标框四角 GPS（用于在地图上绘制目标轮廓）
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
            t[QStringLiteral("locked")] = (cls == 0xB1 || cls == 0xB2);
            t[QStringLiteral("bbox")] = bbox;
            targetArr.append(t);
        } else {
            // 无像素框的目标仅记录位置与类别
            QJsonObject t;
            t[QStringLiteral("id")] = id;
            t[QStringLiteral("cls")] = cls;
            t[QStringLiteral("dist")] = dist;
            t[QStringLiteral("lat")] = tLat;
            t[QStringLiteral("lon")] = tLon;
            t[QStringLiteral("locked")] = false;
            targetArr.append(t);
        }
    }

    m_mapDlg->mapWidget()->updateTargetMarkers(targetArr);
}

//============================================================================
// syncLensTargetByDisplayMode - 根据显示模式自动同步镜头目标
// PipShow 为 1 (红外) 或 4 (大图红外) 时，自动选择红外镜头 (target=1)，
// 否则选择可见光镜头 (target=0)
//============================================================================
void MainWindow::syncLensTargetByDisplayMode(int pipShow)
{
    int target = 0;
    if (pipShow == 1 || pipShow == 4)
        target = 1;

    if (ui->comboLensTarget->currentIndex() != target)
        ui->comboLensTarget->setCurrentIndex(target);
}