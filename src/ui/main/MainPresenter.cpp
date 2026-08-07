#include "mapwidget.h"
#include <QJsonArray>
#include "MainPresenter.h"
#include "mainwindow.h"
#include "service/DeviceManager.h"
#include "core/EventBus.h"
#include "ui_mainwindow.h"
#include "core/GeoCalculator.h"
#include <QMessageBox>
#include <QVariant>
#include "ui/components/VideoGridWidget.h"
#include <QtMath>

MainPresenter::MainPresenter(MainWindow* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_cfg(cfg)
    , m_currentDeviceId("default_device")
{
    // 1. 初始化全局设备管理器
    DeviceManager::instance()->init(m_cfg);
    
    // 2. 预先创建一个默认设备（兼容旧版单设备架构）
    DeviceManager::instance()->addDevice(m_currentDeviceId);
    
    // 3. 挂载事件总线
    setupEventBus();
}

MainPresenter::~MainPresenter()
{
}

void MainPresenter::setupEventBus()
{
    EventBus* bus = EventBus::instance();
    
    // -- 连接事件 --
    // （注意：为了避免目前 MainWindow 还没完全解耦时的编译错误，我们暂时还是保留原有连接，
    //   这里只是铺垫，等 MainWindow 内的代码被剥离后，将会在这里回调 view 的方法）
    
    connect(bus, &EventBus::sigDeviceAiTimeout, this, &MainPresenter::onDeviceAiTimeout);
    // connect(bus, &EventBus::sigDeviceConnected, this, [this](const QString& deviceId) {
    //     if (deviceId == m_currentDeviceId) m_view->onDeviceConnected();
    // 
    connect(bus, &EventBus::sigJsonReceived, this, &MainPresenter::onJsonReceived);
}

void MainPresenter::connectToDevice(const QString& ip, quint16 port)
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        ctx->startConnection(ip, port);
    }
}

void MainPresenter::disconnectDevice()
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        ctx->stopConnection();
    }
}

void MainPresenter::ptzMove(int direction)
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        if (ctx->motorController()) {
            ctx->motorController()->ptzMove(static_cast<PtzDir>(direction));
        }
    }
}

void MainPresenter::ptzStop()
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        if (ctx->motorController()) {
            ctx->motorController()->ptzStop();
        }
    }
}

DeviceController* MainPresenter::motorController() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->motorController();
    }
    return nullptr;
}

TJsonClient* MainPresenter::tcpClient() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->tcpClient();
    }
    return nullptr;
}

RtspThread* MainPresenter::videoStream() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->videoStream();
    }
    return nullptr;
}

PtzForwarder* MainPresenter::ptzForwarder() const
{
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        return ctx->ptzForwarder();
    }
    return nullptr;
}



// --- Extracted from MainWindow ---
void MainPresenter::on_btnConnect_clicked()
{
    if (this->tcpClient()->isConnected()) {
        this->tcpClient()->disconnectDevice();
    } else {
        if (!m_cfg->turntableIpEnabled()) {
             m_view->getUi()->statusbar->showMessage(QString::fromUtf8("转台IP连接已禁用"), 3000);
             return;
        }
        QString ip = m_view->getUi()->lineEditIp->text();
        this->tcpClient()->connectToDevice(ip, m_cfg->deviceTcpPort());
        m_view->getUi()->btnConnect->setText(QString::fromUtf8("连接中..."));
        m_view->getUi()->btnConnect->setEnabled(false);
        m_view->getUi()->btnCancelConnect->setVisible(true);
    }
}

void MainPresenter::on_btnCancelConnect_clicked()
{
    this->tcpClient()->disconnectDevice();
    m_view->getUi()->btnConnect->setText(QString::fromUtf8("连接设备"));
    m_view->getUi()->btnConnect->setEnabled(true);
    m_view->getUi()->btnCancelConnect->setVisible(false);
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("已取消连接"), 3000);
}

void MainPresenter::on_btnVideoConnect_clicked()
{
    QString url = m_view->getUi()->lineEditRtsp->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(m_view, "RTSP", "请输入 RTSP 地址");
        return;
    }
    m_view->m_rtspEverOpened = true;
    this->videoStream()->openStream(url);
    m_view->getUi()->btnVideoConnect->setEnabled(false);
    m_view->getUi()->btnVideoConnect->setText(QString::fromUtf8("连接中..."));
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("正在连接 RTSP 视频流..."));
}

void MainPresenter::on_btnVideoDisconnect_clicked()
{
    if (auto vw = m_view->m_videoGrid->getWidget("default_device")) vw->clearFrame();
    m_view->m_videoGrid->repaint();
    this->videoStream()->closeStream();
    m_view->getUi()->btnVideoConnect->setEnabled(true);
    m_view->getUi()->btnVideoConnect->setText(QString::fromUtf8("开启"));
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("视频已断开"), 3000);
}

void MainPresenter::on_btnPtzMoveTo_clicked()
{
    if (!m_view->requireConnected()) return;

    bool panOk = false;
    bool tiltOk = false;
    double pan = m_view->getUi()->editTargetPan->text().toDouble(&panOk);
    double tilt = m_view->getUi()->editTargetTilt->text().toDouble(&tiltOk);

    if (panOk && tiltOk) {
        this->motorController()->ptzMoveTo(pan, tilt);
    } else {
        QMessageBox::warning(m_view, "输入错误", "请输入有效的水平和垂直角度值。");
    }
}

void MainPresenter::on_btnPtzMoveToGps_clicked()
{
    if (!m_view->requireConnected()) return;

    QString lonStr = m_view->getUi()->editTargetLon->text().trimmed();
    QString latStr = m_view->getUi()->editTargetLat->text().trimmed();
    QString altStr = m_view->getUi()->editTargetAlt->text().trimmed();

    if (lonStr.isEmpty() || latStr.isEmpty()) {
        QMessageBox::warning(m_view, "输入错误", "请输入目标的经纬度和高度。");
        return;
    }

    double targetLon = GeoCalculator::parseCoord(lonStr);
    double targetLat = GeoCalculator::parseCoord(latStr);
    double targetAlt = altStr.toDouble();

    double devLat = GeoCalculator::parseCoord(m_view->getUi()->statLatitude->text());
    double devLon = GeoCalculator::parseCoord(m_view->getUi()->statLongitude->text());
    double devAlt = m_view->m_deviceHeight;

    if (devLat == 0 && devLon == 0) {
        QMessageBox::warning(m_view, "状态错误", "当前设备 GPS 未知，无法计算目标角度。");
        return;
    }

    double pan = GeoCalculator::bearing(devLat, devLon, targetLat, targetLon);
    double dist = GeoCalculator::haversineDistance(devLat, devLon, targetLat, targetLon);

    double tilt = 0;
    if (dist > 0.001) { 
        tilt = -qRadiansToDegrees(qAtan2(targetAlt - devAlt, dist));
    }

    this->motorController()->ptzMoveTo(pan, tilt);
    m_view->getUi()->statusbar->showMessage(QString("转到 GPS: 方位=%1° 俯仰=%2°").arg(pan, 0, 'f', 1).arg(tilt, 0, 'f', 1), 3000);
}

void MainPresenter::on_btnPanZeroCalib_clicked()
{
    if (!m_view->requireConnected()) return;
    
    if (QMessageBox::question(m_view, "零点标定", "确认将当前云台水平和俯仰位置标定为 0 度？") == QMessageBox::Yes) {
        if (m_cfg->softwarePtzCalibrationEnabled()) {
            // 开启了模拟串口服务器，使用软件偏置
            QString panStr = m_view->getUi()->statPanAngle->text();
            panStr.remove("°");
            double displayedPan = panStr.toDouble();

            QString tiltStr = m_view->getUi()->statTiltAngle->text();
            tiltStr.remove("°");
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

            this->ptzForwarder()->setOffsets(newPanOffset, newTiltOffset);
            this->ptzForwarder()->flushZeroPosition();

            m_view->getUi()->statPanAngle->setText("0.0°");
            m_view->getUi()->statTiltAngle->setText("0.0°");
            m_view->getUi()->statusbar->showMessage("零点标定(软件偏置)已保存", 3000);
        } else {
            // 未开启模拟串口服务器，直接通过 PELCO-D 透传标定指令
            this->motorController()->ptzSetZero();
            m_view->getUi()->statusbar->showMessage("零点标定指令(Pelco-D)已下发", 3000);
            // 这里不强制改 UI，让后续设备主动上报的新角度来刷新 UI
        }
    }
}

void MainPresenter::on_btnSetLocation_clicked()
{
    QString latStr = m_view->getUi()->editSetLat->text().trimmed();
    QString lonStr = m_view->getUi()->editSetLon->text().trimmed();

    if (latStr.isEmpty() || lonStr.isEmpty()) {
        QMessageBox::warning(m_view, QString::fromUtf8("输入错误"),
                             QString::fromUtf8("请填写完整的经纬度参数"));
        return;
    }

    if (!m_view->requireConnected()) return;

    double latNum = GeoCalculator::parseCoord(latStr);
    double lonNum = GeoCalculator::parseCoord(lonStr);

    QString altStr = m_view->getUi()->editSetHeight->text().trimmed();
    if (!altStr.isEmpty()) {
        m_view->m_deviceHeight = altStr.toDouble();
        m_view->getUi()->statHeight->setText(QString::number(m_view->m_deviceHeight, 'f', 1) + QStringLiteral(" m"));
    }

    QString strictLat = QString::asprintf("%.7f%s", qAbs(latNum), latNum >= 0 ? "N" : "S");
    QString strictLon = QString::asprintf("%.7f%s", qAbs(lonNum), lonNum >= 0 ? "E" : "W");

    this->motorController()->setLocation(strictLat, strictLon);
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("已下发经纬度"), 3000);
}

void MainPresenter::on_btnGetImageParams_clicked()
{
    if (!m_view->requireConnected()) return;
    this->motorController()->queryImageParams();
    m_view->getUi()->statusbar->showMessage(QString::fromUtf8("已发送参数查询请求"), 3000);
}



void MainPresenter::onDeviceAiTimeout(const QString& deviceId)
{
    if (deviceId != m_currentDeviceId) return;

    auto ui = m_view->getUi();
    ui->lblIdentifyCount->setText(QString::fromUtf8("目标总数: 0"));
    ui->tableIdentify->setRowCount(0);

    ui->lblTrackStatus->setText(QString::fromUtf8("状态: 未锁定"));
    ui->lblTrackStatus->setProperty("state", "nolock");
    MainWindow::refreshStyle(ui->lblTrackStatus);
    ui->trackPos->clear();
    ui->trackMissDistance->clear();
    ui->trackDistance->clear();

    m_view->m_mapWidget->clearAllTracks();
    m_view->m_mapWidget->updateTargetMarkers(QJsonArray());
    m_view->m_mapWidget->clearFov();
}

void MainPresenter::onDeviceDoubleClicked(const QString& name, const QString& ip, const QString& rtspUrl)
{
    QString deviceId = QString("dev_%1").arg(ip);

    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (!ctx) {
        ctx = DeviceManager::instance()->addDevice(deviceId);
    }

    ctx->startConnection(ip, m_cfg->deviceTcpPort());

    VideoWidget* vw = m_view->m_videoGrid->bindDevice(deviceId);

    connect(ctx->videoStream(), &RtspThread::frameReady, m_view,
        [vw](const QImage& frame) {
            vw->setFrame(frame);
        }, Qt::QueuedConnection);

    if (!rtspUrl.isEmpty()) {
        ctx->videoStream()->openStream(rtspUrl);
    }

    m_currentDeviceId = deviceId;
}


void MainPresenter::updateStatusFromJson(const QJsonObject& doc)
{
    QString controlType = doc.value("ControlType").toString();
    CameraConfig& cam = m_cfg->cam();

    //==========================================================================
    // 1) AIInfo - AI 识别与跟踪结果帧
    //==========================================================================
    if (controlType == "AIInfo") {
        m_view->m_lastAiInfoTime = QDateTime::currentDateTime();
        int workMode = doc.value("WorkMode").toInt();
        int count = doc.value("ObjectCount").toInt();

        if (workMode == 1) {
            //==================================================================
            // 识别模式 (WorkMode=1)：
            // 遍历 Object 字典，将每个目标的 ID/类别/距离/像素位置/脱靶量
            // 填入识别结果表格 tableIdentify
            //==================================================================
            m_view->getUi()->lblIdentifyCount->setText(QString::fromUtf8("目标总数: %1").arg(count));
            m_view->getUi()->tableIdentify->setRowCount(0);  // 清空旧数据，重新填充

            // 根据当前显示模式判断使用可见光还是红外参数
            // combo 索引: 0=大图可见光, 1=红外, 2=可见光, 3=融合, 4=大图红外
            bool isVis = (m_view->m_currentPipShow != 1 && m_view->m_currentPipShow != 4);
            double px = isVis ? cam.visPixelSize : cam.irPixelSize;
            double fl = isVis ? cam.visMinFocal * m_view->m_currentVisZoom
                              : cam.irMinFocal * m_view->m_currentIrZoom;
            int halfW = (isVis ? m_view->m_currentResX : cam.irResX) / 2;
            int halfH = (isVis ? m_view->m_currentResY : cam.irResY) / 2;

            // Object 字段是一个字典，key 为目标 ID，value 为目标属性
            if (doc.contains("Object") && doc.value("Object").isObject()) {
                QJsonObject objMap = doc.value("Object").toObject();
                for (auto it = objMap.begin(); it != objMap.end(); ++it) {
                    QString id = it.key();
                    QJsonObject obj = it.value().toObject();

                    int cls = obj.value("Class").toInt();
                    double dist = m_view->calcVisualDistance(obj, cls, false);
                    if (dist > 0) {
                        m_view->m_lastAiDist = dist;
                        m_view->m_lastAiDistEstimated = (obj.value("Distance").toDouble(0) <= 0);
                    }

                    int r = m_view->getUi()->tableIdentify->rowCount();
                    m_view->getUi()->tableIdentify->insertRow(r);
                    m_view->getUi()->tableIdentify->setItem(r, 0, new QTableWidgetItem(id));
                    m_view->getUi()->tableIdentify->setItem(r, 1, new QTableWidgetItem(QString::number(cls)));
                    m_view->getUi()->tableIdentify->setItem(r, 2, new QTableWidgetItem(QString::number(dist, 'f', 1)));

                    if (obj.contains("Points")) {
                        QJsonObject pts = obj.value("Points").toObject();
                        int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                        int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                        QString pos = QString("(%1,%2)").arg(l).arg(t);
                        m_view->getUi()->tableIdentify->setItem(r, 3, new QTableWidgetItem(pos));

                        // 计算目标中心相对于画面中心的脱靶量（毫弧度）
                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        QString miss = GeoCalculator::missMradStr(cx - halfW, cy - halfH, px, fl);
                        m_view->getUi()->tableIdentify->setItem(r, 4, new QTableWidgetItem(miss));
                    }
                }
            }
        }

        // 识别模式与跟踪模式都需要更新地图上的目标标记
        if ((workMode == 1) || (workMode >= 2 && workMode <= 4))
            this->updateMapTargets(doc, workMode);

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
                m_view->getUi()->lblTrackStatus->setText(statusFull);
                m_view->getUi()->lblTrackStatus->setProperty("state", locked ? "locked" : "missed");
                MainWindow::refreshStyle(m_view->getUi()->lblTrackStatus);

                if (obj.contains("Distance")) {
                    double rawDist = obj.value("Distance").toDouble(0);
                    if (rawDist > 0)
                        m_view->getUi()->trackDistance->setText(QString::number(rawDist, 'f', 1) + QStringLiteral(" m"));
                    // rawDist==0: 保留 m_view->calcVisualDistance 设置的估算值
                } else
                    m_view->getUi()->trackDistance->clear();

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    int cx = (l + r2) / 2, cy = (t + b) / 2;
                    int pw = r2 - l, ph = b - t;
                    m_view->getUi()->trackPos->setText(QString("(%1,%2) %3×%4").arg(cx).arg(cy).arg(pw).arg(ph));

                    // 计算脱靶量：像素偏移 × 像元尺寸 / 焦距 → 毫弧度
                    bool isVis = (m_view->m_currentPipShow != 1 && m_view->m_currentPipShow != 4);
                    double px = isVis ? cam.visPixelSize : cam.irPixelSize;
                    double fl = isVis ? cam.visMinFocal * m_view->m_currentVisZoom
                                      : cam.irMinFocal * m_view->m_currentIrZoom;
                    int halfW = (isVis ? m_view->m_currentResX : cam.irResX) / 2;
                    int halfH = (isVis ? m_view->m_currentResY : cam.irResY) / 2;
                    double objCx = (l + r2) / 2.0, objCy = (t + b) / 2.0;
                    double dx = objCx - halfW, dy = objCy - halfH;
                    double dxMrad = dx * px / fl;
                    double dyMrad = dy * px / fl;
                    m_view->getUi()->trackMissDistance->setText(QString("H: %1  V: %2 mrad")
                        .arg(dxMrad, 0, 'f', 2).arg(dyMrad, 0, 'f', 2));
                } else {
                    m_view->getUi()->trackPos->clear();
                    m_view->getUi()->trackMissDistance->clear();
                }
            } else {
                // 无目标：显示"未锁定"并清空所有跟踪字段
                m_view->getUi()->lblTrackStatus->setText(QString::fromUtf8("状态: 未锁定"));
                m_view->getUi()->lblTrackStatus->setProperty("state", "nolock");
                MainWindow::refreshStyle(m_view->getUi()->lblTrackStatus);
                m_view->getUi()->trackPos->clear();
                m_view->getUi()->trackMissDistance->clear();
                m_view->getUi()->trackDistance->clear();
            }
        }

    //==========================================================================
    // 2) ZoomInfo - 镜头倍率与设备状态帧
    // 更新变倍倍率、GPS 坐标、高度、激光测距、云台水平/垂直角
    // 同时触发镜头统计信息更新与地图设备位置更新
    //==========================================================================
    } else if (controlType == "ZoomInfo") {
        m_view->m_currentVisZoom = doc.value("ZoomInfo").toDouble(1.0);
        m_view->m_currentIrZoom = doc.value("ZoomInfoIR").toDouble(1.0);

        m_view->getUi()->statCamMode->setText(QString::number(doc.value("CamShowMode").toInt()));
        m_view->getUi()->statLatitude->setText(doc.value("Latitude").toString());
        m_view->getUi()->statLongitude->setText(doc.value("Longitude").toString());
        {
            double h = doc.value("Height").toDouble();
            if (h != 0.0)
                m_view->getUi()->statHeight->setText(QString::number(h, 'f', 1) + QStringLiteral(" m"));
            else
                m_view->getUi()->statHeight->clear();
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

        m_view->getUi()->statPanAngle->setText(QString::number(rawPan, 'f', 1) + QStringLiteral("°"));
        m_view->m_currentTilt = rawTilt;
        m_view->getUi()->statTiltAngle->setText(QString::number(rawTilt, 'f', 1) + QStringLiteral("°"));

        m_view->updateLensStats();
        this->updateMapDevicePosition(doc);

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
        m_view->getUi()->paramResolution->setText(imgSize >= 0 && imgSize < 4 ? resMap[imgSize] : QString::number(imgSize));
        {
            static const int resTab[][2] = {{1920,1080},{1280,720},{704,576},{2566,1520}};
            if (imgSize >= 0 && imgSize < 4) {
                m_view->m_currentResX = resTab[imgSize][0];
                m_view->m_currentResY = resTab[imgSize][1];
            }
        }

        // 图像码率
        m_view->getUi()->paramBitrate->setText(QString("%1 Kb/s").arg(doc.value("ImageBit").toInt()));

        // 编码格式映射表
        static const char* codecMap[] = {"H264", "H265"};
        int codec = doc.value("ImageCode").toInt();
        m_view->getUi()->paramCodec->setText(codec >= 0 && codec < 2 ? codecMap[codec] : QString::number(codec));

        // 工作模式映射表
        static const char* wmMap[] = {"关闭AI", "识别", "自动跟踪", "点选跟踪", "波门/框选跟踪"};
        int wm = doc.value("WorkMode").toInt();
        m_view->getUi()->paramWorkMode->setText(wm >= 0 && wm < 5 ? QString::fromUtf8(wmMap[wm]) : QString::number(wm));
        m_view->m_previousWorkMode = wm;

        // 显示类型映射表 (PIP = Picture-in-Picture)
        static const char* pipMap[] = {"大图可见光", "红外", "可见光", "融合", "大图红外"};
        int pipRaw = doc.value("PipShow").toInt();
        int comboIdx = DeviceController::pipShowToComboIndex(pipRaw);
        m_view->getUi()->paramPipShow->setText(comboIdx >= 0 && comboIdx < 5 ? QString::fromUtf8(pipMap[comboIdx]) : QString::number(pipRaw));

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
        m_view->getUi()->paramAlgoModel->setText(modelStr.isEmpty() ? QString::number(model) : modelStr);
        m_view->m_previousAlgoModel = model;

        m_view->getUi()->paramMaxVisFL->setText(doc.value("MaxVisFL").toString());
        m_view->getUi()->paramMaxIRFL->setText(doc.value("MaxIRFL").toString());

        m_view->m_currentPipShow = DeviceController::pipShowToComboIndex(doc.value("PipShow").toInt());
        m_view->m_previousDisplayMode = m_view->m_currentPipShow;

        // 同步 UI 下拉框到设备当前值，同时抑制信号递归
        m_view->m_updatingFromDevice = true;
        // 首次连接时同步算法模型下拉框，后续不再覆盖用户选择
        if (!m_view->m_algoModelInitialized) {
            m_view->m_currentAlgoModel = model;
            // 高段 = 传感器类型 (0=可见光, 1=红外) → comboAlgoModel1
            if (high >= 0 && high < m_view->getUi()->comboAlgoModel1->count())
                m_view->getUi()->comboAlgoModel1->setCurrentIndex(high);
            // 低段 = 识别类型 (2-6 → comboAlgoModel2 索引 0-4)
            if (low >= 2 && low <= 6)
                m_view->getUi()->comboAlgoModel2->setCurrentIndex(low - 2);
            m_view->m_algoModelInitialized = true;
        }
        int pipShow = doc.value("PipShow").toInt();
        if (!m_view->m_displayModeInitialized) {
            int comboIdx = DeviceController::pipShowToComboIndex(pipShow);
            if (comboIdx >= 0 && comboIdx < m_view->getUi()->comboDisplayMode->count()) {
                m_view->getUi()->comboDisplayMode->setCurrentIndex(comboIdx);
                m_view->m_displayModeInitialized = true;
            }
        }
        // 首次连接时同步工作模式下拉框，后续不再覆盖用户选择
        if (!m_view->m_workModeInitialized && wm >= 0 && wm < m_view->getUi()->comboWorkMode->count()) {
            m_view->getUi()->comboWorkMode->setCurrentIndex(wm);
            m_view->m_workModeInitialized = true;
        }
        m_view->m_updatingFromDevice = false;
    }
}

//============================================================================
// this->updateMapDevicePosition - 更新地图上的设备位置与视场角
// 从 ZoomInfo JSON 帧中解析 GPS、云台角度、激光测距等数据，
// 计算当前镜头的水平/垂直视场角，绘制到地图控件上
//
// 视场角计算：
//   HFOV = 2 × arctan(传感器宽度_mm / (2 × 焦距_mm))
//   VFOV = HFOV × 9/16 (假定 16:9 传感器宽高比)
// 传感器宽度 = 像元尺寸 × 水平分辨率 / 1000
//============================================================================

// updateMapTargets - 更新地图上的 AI 目标标记
// 跟踪模式 (WorkMode 2~4)：
//   - 仅显示 1 个目标（锁定 0xB1 优先，丢失 0xB2 次之）
//   - 首次失锁（0xB2）时记录时间，保持最后位置 5 秒
//   - 失锁超 5 秒清除轨迹和目标点
// 识别模式 (WorkMode 1)：显示全部识别目标，无上报时清空遗留
//============================================================================
void MainPresenter::updateMapTargets(const QJsonObject& doc, int workMode)
{
    CameraIntrinsics camInfo;
    CameraConfig& camCfg = m_cfg->cam();
    bool isVis = (m_view->m_currentPipShow != 1 && m_view->m_currentPipShow != 4);
    camInfo.pixelSizeUm = isVis ? camCfg.visPixelSize : camCfg.irPixelSize;
    camInfo.focalLengthMm = isVis ? camCfg.visMinFocal * m_view->m_currentVisZoom : camCfg.irMinFocal * m_view->m_currentIrZoom;
    camInfo.resX = isVis ? camCfg.visResX : camCfg.irResX;
    camInfo.resY = isVis ? camCfg.visResY : camCfg.irResY;

    DevicePose devPose;
    devPose.lat = GeoCalculator::parseCoord(m_view->getUi()->statLatitude->text());
    devPose.lon = GeoCalculator::parseCoord(m_view->getUi()->statLongitude->text());
    devPose.panDeg = m_view->getUi()->statPanAngle->text().toDouble();

    bool hasObject = doc.contains("Object") && doc.value("Object").isObject();
    QJsonObject objMap;
    if (hasObject) objMap = doc.value("Object").toObject();
    double tilt = m_view->m_currentTilt;

    //==========================================================================
    // 识别模式 (WorkMode=1)：显示所有目标，无上报时清空
    //==========================================================================
    if (workMode == 1) {
        if (!hasObject || objMap.isEmpty()) {
            m_view->m_mapWidget->clearAllTracks();
            m_view->m_mapWidget->clearFov();
            m_view->m_mapWidget->updateTargetMarkers(QJsonArray());
            return;
        }

        QJsonArray targetArr;
        for (auto it = objMap.begin(); it != objMap.end(); ++it) {
            QString id = it.key();
            QJsonObject obj = it.value().toObject();
            int cls = obj.value("Class").toInt();
            double tLat = 0, tLon = 0;

            double dist = m_view->calcVisualDistance(obj, cls, false);

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
        m_view->m_mapWidget->updateTargetMarkers(targetArr);
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
            m_view->m_track.lostSince = QDateTime();

            int cls = lockedObj.value("Class").toInt();
            double tLat = 0, tLon = 0;

            double dist = m_view->calcVisualDistance(lockedObj, cls, true);
            // 缓存 AI 目标距离（用于 ZoomInfo 无激光测距时回退）
            m_view->m_lastAiDist = dist;
            m_view->m_lastAiDistEstimated = (lockedObj.value("Distance").toDouble(0) <= 0 && dist > 0);

            if (lockedObj.contains("Points")) {
                QJsonObject pts = lockedObj.value("Points").toObject();
                int L = pts.value("Left").toInt(), T = pts.value("Top").toInt();
                int R = pts.value("Right").toInt(), B = pts.value("Bottom").toInt();
                double cx = (L + R) / 2.0, cy = (T + B) / 2.0;
                GeoCalculator::pixelToGps(cx, cy, dist, camInfo, devPose, tLat, tLon);

                m_view->m_track.lat = tLat;
                m_view->m_track.lon = tLon;
                m_view->m_track.cls = cls;

                // 计算速度（米/秒）
                double speed = 0;
                if (m_view->m_track.prevTime.isValid()) {
                    double dist_m = GeoCalculator::haversineDistance(m_view->m_track.prevLat, m_view->m_track.prevLon, tLat, tLon);
                    double dt_s = m_view->m_track.prevTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
                    if (dt_s > 0) speed = dist_m / dt_s;
                }
                m_view->m_track.prevLat = tLat;
                m_view->m_track.prevLon = tLon;
                m_view->m_track.prevTime = QDateTime::currentDateTime();

                // 轨迹点抽稀判定
                if (tLat != 0 && tLon != 0) {
                    // 目标切换时重置抽稀状态，确保新目标首点必定绘制
                    bool targetChanged = (m_view->m_track.id != lockedId);
                    m_view->m_track.id = lockedId;
                    if (targetChanged)
                        m_view->m_track.plotHeading = -1;

                    double outBearing = 0;
                    if (GeoCalculator::shouldPlotTrackPoint(tLat, tLon,
                                             m_view->m_track.plotLat, m_view->m_track.plotLon,
                                             m_view->m_track.plotHeading, m_view->m_track.plotTime,
                                             &outBearing)) {
                        m_view->m_mapWidget->appendTrackPoint(lockedId, tLat, tLon, speed);
                        m_view->m_track.plotLat = tLat;
                        m_view->m_track.plotLon = tLon;
                        m_view->m_track.plotTime = QDateTime::currentDateTime();
                        m_view->m_track.plotHeading = outBearing;
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
                m_view->m_mapWidget->updateTargetMarkers(targetArr);
            }
            return;
        }

        // ---- 有丢失目标 (0xB2) ----
        if (!lostId.isEmpty()) {
            int cls = lostObj.value("Class").toInt();
            double tLat = 0, tLon = 0;
            bool hasPts = false;

            double dist = m_view->calcVisualDistance(lostObj, cls, true);

            if (lostObj.contains("Points")) {
                QJsonObject pts = lostObj.value("Points").toObject();
                int L = pts.value("Left").toInt(), T = pts.value("Top").toInt();
                int R = pts.value("Right").toInt(), B = pts.value("Bottom").toInt();
                double cx = (L + R) / 2.0, cy = (T + B) / 2.0;
                GeoCalculator::pixelToGps(cx, cy, dist, camInfo, devPose, tLat, tLon);
                hasPts = true;
            }

            // 首次失锁：保存位置，记录时间
            if (m_view->m_track.lostSince.isNull()) {
                m_view->m_track.id = lostId;
                m_view->m_track.lat = tLat;
                m_view->m_track.lon = tLon;
                m_view->m_track.cls = cls;
                m_view->m_track.lostSince = QDateTime::currentDateTime();
            }

            // 检查是否超过 5 秒
            qint64 elapsed = m_view->m_track.lostSince.msecsTo(QDateTime::currentDateTime());
            if (elapsed >= 5000) {
                // 超过 5 秒，清除轨迹和目标
                m_view->m_mapWidget->clearAllTracks();
                m_view->m_mapWidget->updateTargetMarkers(QJsonArray());
                return;
            }

            // 5 秒内：显示最后位置
            if (hasPts) {
                QJsonArray targetArr;
                QJsonObject t;
                t[QStringLiteral("id")] = m_view->m_track.id;
                t[QStringLiteral("cls")] = m_view->m_track.cls;
                t[QStringLiteral("dist")] = dist;
                t[QStringLiteral("lat")] = m_view->m_track.lat;
                t[QStringLiteral("lon")] = m_view->m_track.lon;
                t[QStringLiteral("locked")] = false;
                t[QStringLiteral("speed")] = 0;
                targetArr.append(t);
                m_view->m_mapWidget->updateTargetMarkers(targetArr);
            }
            return;
        }

        // ---- 有 Object 但无 0xB1/0xB2，清空 ----
        m_view->m_mapWidget->clearAllTracks();
        m_view->m_mapWidget->updateTargetMarkers(QJsonArray());
    }
}

//============================================================================
// updateLensStats - 更新镜头统计数据
// 根据当前变倍倍率计算可见光与红外的：
//   - 当前焦距 (最小焦距 × 倍率)
//   - 水平视场角 (HFOV): 2 × arctan(传感器宽度 / (2 × 焦距))
// 传感器宽度 = 像元尺寸 × 水平分辨率 (单位换算为 mm)
//============================================================================

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

void MainPresenter::updateMapDevicePosition(const QJsonObject& doc)
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
        range = m_view->m_lastAiDist;
        rangeEstimated = m_view->m_lastAiDistEstimated;
    }

    qDebug() << "[MapPos] raw:" << latStr << lonStr << "parsed:" << lat << lon;
    if (lat == 0 && lon == 0) return;

    m_view->m_mapWidget->setDevicePosition(lat, lon);

    // 计算可见光视场角
    CameraConfig& cam = m_cfg->cam();
    double visSensorW = cam.visPixelSize * cam.visResX / 1000.0;
    double visFocal = cam.visMinFocal * m_view->m_currentVisZoom;
    double visHfov = 2.0 * qAtan(visSensorW / (2.0 * visFocal)) * 180.0 / M_PI;
    double visVfov = visHfov * cam.visResY / cam.visResX;

    // 计算红外视场角
    double irSensorW = cam.irPixelSize * cam.irResX / 1000.0;
    double irFocal = cam.irMinFocal * m_view->m_currentIrZoom;
    double irHfov = 2.0 * qAtan(irSensorW / (2.0 * irFocal)) * 180.0 / M_PI;
    double irVfov = irHfov * cam.irResY / cam.irResX;

    // 可见光视场角 4km（蓝色），红外视场角 2km（红色）
    m_view->m_mapWidget->setVisFov(lat, lon, pan, tilt, visHfov, visVfov, m_cfg->visFovDistance());
    m_view->m_mapWidget->setIrFov(lat, lon, pan, tilt, irHfov, irVfov, m_cfg->irFovDistance());
    m_view->m_mapWidget->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, rangeEstimated);
}

//============================================================================


void MainPresenter::onJsonReceived(const QString& deviceId, const QJsonObject& doc) {
    if (deviceId == m_currentDeviceId) {
        updateStatusFromJson(doc);
    }
}
