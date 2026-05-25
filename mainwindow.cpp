#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "rtspthread.h"
#include "videowidget.h"
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QtMath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_client(new TJsonClient(this))
    , m_cfg(new ConfigManager(this))
    , m_device(new DeviceController(m_client, m_cfg, this))
    , m_rtsp(new RtspThread(this))
{
    ui->setupUi(this);
    setupUiStyles();

    // ── RTSP video ──
    connect(m_rtsp, &RtspThread::frameReady, this, &MainWindow::onRtspFrame);
    connect(m_rtsp, &RtspThread::streamOpened, this, &MainWindow::onRtspOpened);
    connect(m_rtsp, &RtspThread::streamError, this, &MainWindow::onRtspError);
    connect(ui->videoWidget, &VideoWidget::selectionFinished, this, &MainWindow::onVideoSelection);

    // ── T-JSON control ──
    connect(m_client, &TJsonClient::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(m_client, &TJsonClient::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);
    connect(m_client, &TJsonClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_client, &TJsonClient::jsonReceived, this, &MainWindow::onJsonReceived);
    connect(m_client, &TJsonClient::imageSnapped, this, &MainWindow::onImageSnapped);
    
    connect(m_client, &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        Q_UNUSED(maxRetries);
        ui->btnConnect->setText(QString::fromUtf8("重连中(次数:%1)").arg(attempt));
        ui->btnConnect->setStyleSheet("background-color: #d68b00;");
        ui->statusbar->showMessage(QString::fromUtf8("网络波动，正在进行第 %1 次自动探测重连...").arg(attempt));
    });
    connect(m_client, &TJsonClient::reconnectFailed, this, [this]() {
        ui->btnConnect->setText(QString::fromUtf8("连接设备"));
        ui->btnConnect->setEnabled(true);
        ui->btnConnect->setStyleSheet("");
        ui->statusbar->showMessage(QString::fromUtf8("重连失败，已放弃连接"), 5000);
    });

    // ================= 云台八方向控制 (Pelco-D) =================
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

    // ================= 云台速度滑动条 =================
    ui->sliderSpeed->setValue(m_cfg->ptz().panSpeed);
    ui->spinSpeed->setValue(m_cfg->ptz().panSpeed);

    connect(ui->sliderSpeed, &QSlider::valueChanged, ui->spinSpeed, &QSpinBox::setValue);
    connect(ui->spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderSpeed, &QSlider::setValue);
    connect(ui->spinSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        m_cfg->ptz().panSpeed = static_cast<quint8>(val);
        m_cfg->ptz().tiltSpeed = static_cast<quint8>(val);
        m_cfg->save();
    });

    // ================= 镜头控制 (Zoom / Focus) =================
    auto getTarget = [this]() { return ui->comboLensTarget->currentIndex(); };
    auto connectLensBtn = [this, &getTarget](QPushButton* btn, int op) {
        connect(btn, &QPushButton::pressed, this, [this, op, &getTarget]() {
            int t = getTarget();
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

    // ================= 镜头速度滑动条 =================
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

    // ================= 预置位控制 =================
    connect(ui->btnCallPreset, &QPushButton::clicked, this, [this]() {
        m_device->callPreset(ui->spinPreset->value());
    });
    connect(ui->btnSetPreset, &QPushButton::clicked, this, [this]() {
        m_device->setPreset(ui->spinPreset->value());
    });
    connect(ui->btnDelPreset, &QPushButton::clicked, this, [this]() {
        m_device->delPreset(ui->spinPreset->value());
    });

    // ================= 附加功能开关 =================
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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiStyles()
{
    QFile qssFile(":/style.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString qss = QLatin1String(qssFile.readAll());
        this->setStyleSheet(qss);
        qssFile.close();
    } else {
        qDebug() << "Failed to load style.qss";
    }
    
    ui->tableIdentify->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

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
    }
}

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

void MainWindow::on_btnVideoDisconnect_clicked()
{
    m_rtsp->closeStream();
    ui->btnVideoConnect->setEnabled(true);
    ui->btnVideoConnect->setText(QString::fromUtf8("连视频"));
    ui->statusbar->showMessage(QString::fromUtf8("视频已断开"), 3000);
}

void MainWindow::onRtspFrame(const QImage &frame)
{
    ui->videoWidget->setFrame(frame);
}

void MainWindow::onRtspOpened()
{
    ui->btnVideoConnect->setEnabled(false);
    ui->btnVideoConnect->setText(QString::fromUtf8("已连接"));
    ui->statusbar->showMessage(QString::fromUtf8("RTSP 视频已连接"), 3000);
}

void MainWindow::onRtspError(const QString &msg)
{
    ui->btnVideoConnect->setEnabled(true);
    ui->btnVideoConnect->setText(QString::fromUtf8("连视频"));
    ui->statusbar->showMessage(msg);
}

void MainWindow::onVideoSelection(const QRectF &normRect)
{
    // normRect: [0,1] normalized to display area
    // Calculate center + width/height for box tracking
    double cx = (normRect.left() + normRect.right()) / 2.0;
    double cy = (normRect.top() + normRect.bottom()) / 2.0;
    double w = normRect.width();
    double h = normRect.height();
    ui->statusbar->showMessage(
        QString::fromUtf8("框选跟踪: 中心(%1,%2) 宽%3高%4")
            .arg(cx, 0, 'f', 3).arg(cy, 0, 'f', 3)
            .arg(w, 0, 'f', 3).arg(h, 0, 'f', 3));
    // TODO: send box tracking command via T-JSON
}

void MainWindow::onDeviceConnected()
{
    ui->btnConnect->setText(QString::fromUtf8("断开连接"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("background-color: #c75450;");
    ui->statusbar->showMessage(QString::fromUtf8("已连接到设备"), 3000);
}

void MainWindow::onDeviceDisconnected()
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("");
    ui->statusbar->showMessage(QString::fromUtf8("设备已断开"), 3000);
}

void MainWindow::onErrorOccurred(const QString& errorMsg)
{
    ui->btnConnect->setText(QString::fromUtf8("连接设备"));
    ui->btnConnect->setEnabled(true);
    ui->btnConnect->setStyleSheet("");
    QMessageBox::warning(this, QString::fromUtf8("连接错误"), errorMsg);
}

void MainWindow::onJsonReceived(const QJsonObject& doc)
{
    updateStatusFromJson(doc);
}

void MainWindow::onImageSnapped(const QByteArray& jpegData, const QRect& location)
{
    Q_UNUSED(jpegData);
    Q_UNUSED(location);
}

void MainWindow::updateStatusFromJson(const QJsonObject& doc)
{
    QString controlType = doc.value("ControlType").toString();
    CameraConfig& cam = m_cfg->cam();

    if (controlType == "AIInfo") {
        int workMode = doc.value("WorkMode").toInt();
        int count = doc.value("ObjectCount").toInt();

        if (workMode == 1) {
            // ========== 识别模式 ==========
            ui->lblIdentifyCount->setText(QString::fromUtf8("目标总数: %1").arg(count));
            ui->tableIdentify->setRowCount(0);

            bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
            double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                              : cam.irMinFocal * m_currentIrZoom;
            int halfW = (isVis ? cam.visResX : cam.irResX) / 2;
            int halfH = (isVis ? cam.visResY : cam.irResY) / 2;

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

                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        QString miss = missMradStr(cx - halfW, cy - halfH, px, fl);
                        ui->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
                    }
                }
            }
        } else if (workMode == 2) {
            // ========== 跟踪模式 ==========
            if (count > 0 && doc.contains("Object") && doc.value("Object").isObject()) {
                QJsonObject objMap = doc.value("Object").toObject();
                QJsonObject obj = objMap.begin().value().toObject();
                int cls = obj.value("Class").toInt();

                bool locked = (cls == 0xB1);
                QString statusText = locked ? QString::fromUtf8("锁定中") : QString::fromUtf8("丢失");
                QString statusFull = QString::fromUtf8("状态: %1").arg(statusText);
                ui->lblTrackStatus->setText(statusFull);
                ui->lblTrackStatus->setStyleSheet(locked ? "color: #00cc00; font-weight: bold;"
                                                         : "color: #cc0000; font-weight: bold;");

                ui->trackId->setText(statusText);
                ui->trackClass->setText(QString::number(cls));
                ui->trackDistance->setText(QString::number(obj.value("Distance").toDouble(), 'f', 1));
                ui->trackAngle->clear();

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    ui->trackPos->setText(QString("(%1,%2)-(%3,%4)").arg(l).arg(t).arg(r2).arg(b));

                    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
                    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
                    double fl = isVis ? cam.visMinFocal * m_currentVisZoom
                                      : cam.irMinFocal * m_currentIrZoom;
                    int halfW = (isVis ? cam.visResX : cam.irResX) / 2;
                    int halfH = (isVis ? cam.visResY : cam.irResY) / 2;
                    double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                    ui->trackMissDistance->setText(missMradStr(cx - halfW, cy - halfH, px, fl));
                }
            } else {
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

    } else if (controlType == "ImageSetting") {
        ui->paramResolution->setText(QString::number(doc.value("ImageSize").toInt()));
        ui->paramBitrate->setText(QString::number(doc.value("ImageBit").toInt()));
        ui->paramCodec->setText(QString::number(doc.value("ImageCode").toInt()));
        ui->paramWorkMode->setText(QString::number(doc.value("WorkMode").toInt()));
        ui->paramPipShow->setText(QString::number(doc.value("PipShow").toInt()));
        ui->paramAlgoModel->setText(QString::number(doc.value("Model").toInt()));
        ui->paramMaxVisFL->setText(doc.value("MaxVisFL").toString());
        ui->paramMaxIRFL->setText(doc.value("MaxIRFL").toString());

        m_currentPipShow = doc.value("PipShow").toInt();

        m_updatingFromDevice = true;
        int model = doc.value("Model").toInt();
        if (model >= 0 && model < ui->comboAlgoModel->count())
            ui->comboAlgoModel->setCurrentIndex(model);
        int pipShow = doc.value("PipShow").toInt();
        if (pipShow >= 0 && pipShow < ui->comboDisplayMode->count())
            ui->comboDisplayMode->setCurrentIndex(pipShow);
        syncLensTargetByDisplayMode(pipShow);
        m_updatingFromDevice = false;
    }
}

void MainWindow::updateLensStats()
{
    CameraConfig& cam = m_cfg->cam();
    const double kRad2Deg = 180.0 / 3.14159265358979323846;

    double visFocal = cam.visMinFocal * m_currentVisZoom;
    double irFocal  = cam.irMinFocal * m_currentIrZoom;

    ui->statZoomVis->setText(QString::number(m_currentVisZoom, 'f', 1));
    ui->statFocalVis->setText(QString::number(visFocal, 'f', 1));
    ui->statFocusVis->clear();

    double visHfov = 2.0 * qAtan((cam.visPixelSize * cam.visResX / 1000.0) / (2.0 * visFocal));
    ui->statFovVis->setText(QString::number(visHfov * kRad2Deg, 'f', 1));

    ui->statZoomIR->setText(QString::number(m_currentIrZoom, 'f', 1));
    ui->statFocalIR->setText(QString::number(irFocal, 'f', 1));
    ui->statFocusIR->clear();

    double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));
    ui->statFovIR->setText(QString::number(irHfov * kRad2Deg, 'f', 1));
}

QString MainWindow::missMradStr(double dx, double dy, double pixelSizeUm, double focalMm)
{
    if (focalMm < 0.1) return QString();
    double dxMrad = dx * pixelSizeUm / focalMm;
    double dyMrad = dy * pixelSizeUm / focalMm;
    double total = qSqrt(dxMrad * dxMrad + dyMrad * dyMrad);
    return QString::number(total, 'f', 2);
}

void MainWindow::on_radioModeOff_clicked() { m_device->setWorkMode(0); }
void MainWindow::on_radioModeIdentify_clicked() { m_device->setWorkMode(1); }
void MainWindow::on_radioModeAutoTrack_clicked() { m_device->setWorkMode(2); }

void MainWindow::on_btnPtzMoveTo_clicked()
{
}

void MainWindow::on_btnSettings_clicked()
{
    SettingsDialog dlg(this);
    dlg.exec();
    m_cfg->reload();
}

void MainWindow::on_comboAlgoModel_currentIndexChanged(int index)
{
    if (m_updatingFromDevice) return;
    m_device->setAlgoModel(index);
}

void MainWindow::on_comboDisplayMode_currentIndexChanged(int index)
{
    if (m_updatingFromDevice) return;
    m_device->setDisplayMode(index);
}

void MainWindow::on_comboLensTarget_currentIndexChanged(int index)
{
    Q_UNUSED(index);
}

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

void MainWindow::syncLensTargetByDisplayMode(int pipShow)
{
    int target = 0;
    if (pipShow == 1 || pipShow == 4)
        target = 1;

    if (ui->comboLensTarget->currentIndex() != target)
        ui->comboLensTarget->setCurrentIndex(target);
}