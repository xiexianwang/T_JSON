#include "ui/views/mapwidget.h"
#include <QJsonArray>
#include "MainPresenter.h"
#include "IMainView.h"
#include "service/DeviceManager.h"
#include "core/EventBus.h"
#include "core/GeoCalculator.h"
#include <QMessageBox>
#include <QVariant>
#include <QTimer>
#include "ui/components/VideoGridWidget.h"
#include "ui/views/videowidget.h"
#include <QtMath>

MainPresenter::MainPresenter(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_cfg(cfg)
    , m_currentDeviceId("default_device")
{
    // 1. 初始化全局设备管理器
    DeviceManager::instance()->init(m_cfg);
    
    // 2. 预先创建一个默认设备（兼容旧版单设备架构）
    DeviceManager::instance()->addDevice(m_currentDeviceId);

    // 2.5 转发默认设备的电机/指令日志信号给 View（避免 View 直连底层组件）
    if (DeviceContext* ctx = DeviceManager::instance()->getDevice(m_currentDeviceId)) {
        if (DeviceController* mc = ctx->motorController()) {
            connect(mc, &DeviceController::motorModeResult, this, &MainPresenter::motorModeChanged);
            connect(mc, &DeviceController::motorSerialError, this, &MainPresenter::motorSerialErrorOccurred);
            connect(mc, &DeviceController::motorSilentResult, this, &MainPresenter::motorSilentChanged);
            connect(mc, &DeviceController::commandSent, this, &MainPresenter::commandSentToLog);
        }
    }
    
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
    
    connect(bus, &EventBus::sigDeviceAiTimeout, this, &MainPresenter::onDeviceAiTimeout);
    connect(bus, &EventBus::sigDeviceFrameReady, this, [this](const QString& deviceId, const QImage& frame) {
        if (VideoWidget* vw = m_view->videoWidget(deviceId)) {
            vw->setFrame(frame);
        }
    });

    connect(bus, &EventBus::sigDeviceConnected, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) {
            // 初始化设备参数
            motorController()->queryImageParams();
            motorController()->setDigitalZoom(m_cfg->digitalZoomEnabled());
            motorController()->setAutoZoom(m_cfg->autoZoomEnabled());
            motorController()->setCaptureUpload(m_cfg->captureUploadEnabled());
            motorController()->posReset(m_cfg->posResetEnabled());
            
            // 通知 UI 更新
            m_view->onDeviceConnected();
        }
    });

    connect(bus, &EventBus::sigDeviceDisconnected, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) m_view->onDeviceDisconnected();
    });
    
    connect(bus, &EventBus::sigRtspOpened, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) m_view->onRtspOpened();
    });
    connect(bus, &EventBus::sigRtspError, this, [this](const QString& deviceId, const QString& msg) {
        if (deviceId == m_currentDeviceId) m_view->onRtspError(msg);
    });
    
    connect(bus, &EventBus::sigDeviceStateUpdated, this, &MainPresenter::onDeviceStateUpdated);
    connect(bus, &EventBus::sigDeviceAiInfoUpdated, this, &MainPresenter::onDeviceAiInfoUpdated);
    connect(bus, &EventBus::sigDeviceError, this, [this](const QString& deviceId, const QString& errorMsg) {
        if (deviceId == m_currentDeviceId) m_view->onErrorOccurred(errorMsg);
    });
    connect(bus, &EventBus::sigImageSnapped, this, [this](const QString& deviceId, const QByteArray& jpegData, const QRect& location) {
        if (deviceId == m_currentDeviceId) m_view->onImageSnapped(jpegData, location);
    });
    connect(bus, &EventBus::sigAckReceived, this, [this](const QString& deviceId, quint8 statusCode) {
        if (deviceId == m_currentDeviceId) showAck(statusCode);
    });
    connect(bus, &EventBus::sigDeviceReconnecting, this, [this](const QString& deviceId, int attempt, int maxRetries) {
        if (deviceId == m_currentDeviceId) m_view->onDeviceReconnecting(attempt, maxRetries);
    });
    connect(bus, &EventBus::sigDeviceReconnectFailed, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) m_view->onDeviceReconnectFailed();
    });
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

void MainPresenter::lensMove(int op)
{
    if (!motorController()) return;
    // 镜头目标根据显示模式自动判断：PipShow 1/4=红外(target=1)，其余=可见光(target=0)
    int target = (m_currentPipShow == 1 || m_currentPipShow == 4) ? 1 : 0;
    switch (op) {
    case 0: motorController()->lensZoomIn(target); break;
    case 1: motorController()->lensZoomOut(target); break;
    case 2: motorController()->lensFocusIn(target); break;
    case 3: motorController()->lensFocusOut(target); break;
    default: break;
    }
}

void MainPresenter::lensStop()
{
    if (motorController()) motorController()->lensStop();
}

void MainPresenter::initPtzForwarder()
{
    if (!m_cfg->serialServerEnabled()) return;
    if (!ptzForwarder()) return;
    ptzForwarder()->start(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
    ptzForwarder()->setOffsets(m_cfg->ptzPanOffset(), m_cfg->ptzTiltOffset());
}

bool MainPresenter::isMotorSerialOpen() const
{
    return motorController() && motorController()->isMotorSerialOpen();
}

bool MainPresenter::isMotorTcpOpen() const
{
    return motorController() && motorController()->isMotorTcpOpen();
}

bool MainPresenter::isVideoStreamRunning() const
{
    return videoStream() && videoStream()->isRunning();
}

void MainPresenter::closeVideoStream()
{
    if (videoStream()) videoStream()->closeStream();
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
             m_view->showStatusMessage(QString::fromUtf8("转台IP连接已禁用"), 3000);
             return;
        }
        QString ip = m_view->ipText();
        this->tcpClient()->connectToDevice(ip, m_cfg->deviceTcpPort());
        m_view->setConnectButton(QString::fromUtf8("连接中..."), false, QString(), true);
    }
}

void MainPresenter::on_btnCancelConnect_clicked()
{
    this->tcpClient()->disconnectDevice();
    m_view->setConnectButton(QString::fromUtf8("连接设备"), true, QString(), false);
    m_view->showStatusMessage(QString::fromUtf8("已取消连接"), 3000);
}

void MainPresenter::on_btnVideoConnect_clicked()
{
    QString url = m_view->rtspUrlText().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(m_view->asWidget(), "RTSP", "请输入 RTSP 地址");
        return;
    }
    m_rtspEverOpened = true;
    this->videoStream()->openStream(url);
    m_view->setVideoConnectButton(QString::fromUtf8("连接中..."), false);
    m_view->showStatusMessage(QString::fromUtf8("正在连接 RTSP 视频流..."));
}

void MainPresenter::on_btnVideoDisconnect_clicked()
{
    if (auto vw = m_view->videoWidget(m_currentDeviceId)) vw->clearFrame();
    m_view->repaintVideoGrid();
    this->videoStream()->closeStream();
    m_view->setVideoConnectButton(QString::fromUtf8("开启"), true);
    m_view->showStatusMessage(QString::fromUtf8("视频已断开"), 3000);
}

void MainPresenter::on_btnPtzMoveTo_clicked()
{
    if (!m_view->requireConnected()) return;

    bool panOk = false;
    bool tiltOk = false;
    double pan = m_view->targetPanText().toDouble(&panOk);
    double tilt = m_view->targetTiltText().toDouble(&tiltOk);

    if (panOk && tiltOk) {
        this->motorController()->ptzMoveTo(pan, tilt);
    } else {
        QMessageBox::warning(m_view->asWidget(), "输入错误", "请输入有效的水平和垂直角度值。");
    }
}

void MainPresenter::on_btnPtzMoveToGps_clicked()
{
    if (!m_view->requireConnected()) return;

    QString lonStr = m_view->targetLonText().trimmed();
    QString latStr = m_view->targetLatText().trimmed();
    QString altStr = m_view->targetAltText().trimmed();

    if (lonStr.isEmpty() || latStr.isEmpty()) {
        QMessageBox::warning(m_view->asWidget(), "输入错误", "请输入目标的经纬度和高度。");
        return;
    }

    double targetLon = GeoCalculator::parseCoord(lonStr);
    double targetLat = GeoCalculator::parseCoord(latStr);
    double targetAlt = altStr.toDouble();

    double devLat = GeoCalculator::parseCoord(m_view->statLatitudeText());
    double devLon = GeoCalculator::parseCoord(m_view->statLongitudeText());
    double devAlt = m_deviceHeight;

    if (devLat == 0 && devLon == 0) {
        QMessageBox::warning(m_view->asWidget(), "状态错误", "当前设备 GPS 未知，无法计算目标角度。");
        return;
    }

    double pan = GeoCalculator::bearing(devLat, devLon, targetLat, targetLon);
    double dist = GeoCalculator::haversineDistance(devLat, devLon, targetLat, targetLon);

    double tilt = 0;
    if (dist > 0.001) { 
        tilt = -qRadiansToDegrees(qAtan2(targetAlt - devAlt, dist));
    }

    this->motorController()->ptzMoveTo(pan, tilt);
    m_view->showStatusMessage(QString("转到 GPS: 方位=%1° 俯仰=%2°").arg(pan, 0, 'f', 1).arg(tilt, 0, 'f', 1), 3000);
}

void MainPresenter::on_btnPanZeroCalib_clicked()
{
    if (!m_view->requireConnected()) return;
    
    if (QMessageBox::question(m_view->asWidget(), "零点标定", "确认将当前云台水平和俯仰位置标定为 0 度？") == QMessageBox::Yes) {
        if (m_cfg->softwarePtzCalibrationEnabled()) {
            // 开启了模拟串口服务器，使用软件偏置
            QString panStr = m_view->statPanAngleText();
            panStr.remove("°");
            double displayedPan = panStr.toDouble();

            QString tiltStr = m_view->statTiltAngleText();
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

            m_view->showDeviceState(-1, QString(), QString(), QString(), "0.0°", "0.0°");
            m_view->showStatusMessage("零点标定(软件偏置)已保存", 3000);
        } else {
            // 未开启模拟串口服务器，直接通过 PELCO-D 透传标定指令
            this->motorController()->ptzSetZero();
            m_view->showStatusMessage("零点标定指令(Pelco-D)已下发", 3000);
            // 这里不强制改 UI，让后续设备主动上报的新角度来刷新 UI
        }
    }
}

void MainPresenter::on_btnSetLocation_clicked()
{
    QString latStr = m_view->setLatText().trimmed();
    QString lonStr = m_view->setLonText().trimmed();

    if (latStr.isEmpty() || lonStr.isEmpty()) {
        QMessageBox::warning(m_view->asWidget(), QString::fromUtf8("输入错误"),
                             QString::fromUtf8("请填写完整的经纬度参数"));
        return;
    }

    if (!m_view->requireConnected()) return;

    double latNum = GeoCalculator::parseCoord(latStr);
    double lonNum = GeoCalculator::parseCoord(lonStr);

    QString altStr = m_view->setHeightText().trimmed();
    if (!altStr.isEmpty()) {
        m_deviceHeight = altStr.toDouble();
        m_view->showDeviceState(-1, QString(), QString(),
                                QString::number(m_deviceHeight, 'f', 1) + QStringLiteral(" m"),
                                QString(), QString());
    }

    QString strictLat = QString::asprintf("%.7f%s", qAbs(latNum), latNum >= 0 ? "N" : "S");
    QString strictLon = QString::asprintf("%.7f%s", qAbs(lonNum), lonNum >= 0 ? "E" : "W");

    this->motorController()->setLocation(strictLat, strictLon);
    m_view->showStatusMessage(QString::fromUtf8("已下发经纬度"), 3000);
}

void MainPresenter::on_btnGetImageParams_clicked()
{
    if (!m_view->requireConnected()) return;
    this->motorController()->queryImageParams();
    m_view->showStatusMessage(QString::fromUtf8("已发送参数查询请求"), 3000);
}



void MainPresenter::onDeviceAiTimeout(const QString& deviceId)
{
    if (deviceId != m_currentDeviceId) return;

    m_view->setIdentifyCount(QString::fromUtf8("目标总数: 0"));
    m_view->clearIdentifyTable();

    m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
    m_view->setTrackPos(QString());
    m_view->setTrackMissDistance(QString());
    m_view->setTrackDistance(QString());

    MapWidget* map = m_view->mapWidget();
    map->clearAllTracks();
    map->updateTargetMarkers(QJsonArray());
    map->clearFov();
}

void MainPresenter::onDeviceDoubleClicked(const QString& name, const QString& ip, const QString& rtspUrl)
{
    QString deviceId = QString("dev_%1").arg(ip);

    m_currentDeviceId = deviceId;

    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (!ctx) {
        ctx = DeviceManager::instance()->addDevice(deviceId);
    }

    m_view->videoWidget(deviceId);

    ctx->startConnection(ip, m_cfg->deviceTcpPort());

    if (!rtspUrl.isEmpty()) {
        ctx->videoStream()->openStream(rtspUrl);
    }
}


void MainPresenter::updateAiInfoFromJson(const QJsonObject& doc)
{
    CameraConfig& cam = m_cfg->cam();

    //==========================================================================
    // AIInfo - AI 识别与跟踪结果帧
    //==========================================================================
    {
        m_lastAiInfoTime = QDateTime::currentDateTime();
        int workMode = doc.value("WorkMode").toInt();
        int count = doc.value("ObjectCount").toInt();

        if (workMode == 1) {
            //==================================================================
            // 识别模式 (WorkMode=1)：
            // 遍历 Object 字典，将每个目标的 ID/类别/距离/像素位置/脱靶量
            // 填入识别结果表格 tableIdentify
            //==================================================================
            m_view->setIdentifyCount(QString::fromUtf8("目标总数: %1").arg(count));
            m_view->clearIdentifyTable();

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

                    QString pos, miss;
                    if (obj.contains("Points")) {
                        QJsonObject pts = obj.value("Points").toObject();
                        int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                        int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                        pos = QString("(%1,%2)").arg(l).arg(t);

                        // 计算目标中心相对于画面中心的脱靶量（毫弧度）
                        double cx = (l + r2) / 2.0, cy = (t + b) / 2.0;
                        miss = GeoCalculator::missMradStr(cx - halfW, cy - halfH, px, fl);
                    }
                    m_view->addIdentifyRow(id, cls, dist, pos, miss);
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
                m_view->showTrackStatus(statusFull, locked ? "locked" : "missed");

                if (obj.contains("Distance")) {
                    double rawDist = obj.value("Distance").toDouble(0);
                    if (rawDist > 0)
                        m_view->setTrackDistance(QString::number(rawDist, 'f', 1) + QStringLiteral(" m"));
                    // rawDist==0: 保留 calcVisualDistance 设置的估算值
                } else
                    m_view->setTrackDistance(QString());

                if (obj.contains("Points")) {
                    QJsonObject pts = obj.value("Points").toObject();
                    int l = pts.value("Left").toInt(), t = pts.value("Top").toInt();
                    int r2 = pts.value("Right").toInt(), b = pts.value("Bottom").toInt();
                    int cx = (l + r2) / 2, cy = (t + b) / 2;
                    int pw = r2 - l, ph = b - t;
                    m_view->setTrackPos(QString("(%1,%2) %3×%4").arg(cx).arg(cy).arg(pw).arg(ph));

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
                    m_view->setTrackMissDistance(QString("H: %1  V: %2 mrad")
                        .arg(dxMrad, 0, 'f', 2).arg(dyMrad, 0, 'f', 2));
                } else {
                    m_view->setTrackPos(QString());
                    m_view->setTrackMissDistance(QString());
                }
            } else {
                // 无目标：显示"未锁定"并清空所有跟踪字段
                m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
                m_view->setTrackPos(QString());
                m_view->setTrackMissDistance(QString());
                m_view->setTrackDistance(QString());
            }
        }
    }
}

void MainPresenter::updateStatusFromState(const DeviceState& state)
{
    CameraConfig& cam = m_cfg->cam();

    //==========================================================================
    // 1) ZoomInfo - 镜头倍率与设备状态帧
    // 更新变倍倍率、GPS 坐标、高度、激光测距、云台水平/垂直角
    // 同时触发镜头统计信息更新与地图设备位置更新
    //==========================================================================
    {
        m_currentVisZoom = state.currentVisZoom;
        m_currentIrZoom = state.currentIrZoom;

        double h = state.altitude;
        QString heightStr;
        if (h != 0.0)
            heightStr = QString::number(h, 'f', 1) + QStringLiteral(" m");

        double rawPan = state.currentPan;
        double rawTilt = state.currentTilt;

        if (m_cfg->softwarePtzCalibrationEnabled()) {
            rawPan -= m_cfg->ptzPanOffset();
            while (rawPan < 0) rawPan += 360.0;
            while (rawPan >= 360.0) rawPan -= 360.0;

            rawTilt -= m_cfg->ptzTiltOffset();
            while (rawTilt < -180.0) rawTilt += 360.0;
            while (rawTilt > 180.0) rawTilt -= 360.0;
        }

        m_currentTilt = rawTilt;
        m_view->showDeviceState(state.camShowMode, state.latitudeRaw, state.longitudeRaw,
                                heightStr,
                                QString::number(rawPan, 'f', 1) + QStringLiteral("°"),
                                QString::number(rawTilt, 'f', 1) + QStringLiteral("°"));

        this->updateLensStats();
        this->updateMapDevicePosition(state);
    }

    //==========================================================================
    // 2) ImageSetting - 图像参数配置帧
    // 设备主动推送或响应查询，更新分辨率/码率/编码/工作模式/显示模式/算法
    // 并根据设备当前值同步 UI 下拉框，同时设置 m_updatingFromDevice 标志
    // 防止 UI 变化再次触发设备指令造成死循环
    //==========================================================================
    {
        // 图像分辨率映射表
        static const char* resMap[] = {"1080P", "720P", "D1", "1440P"};
        int imgSize = state.imgSize;
        QString resStr = (imgSize >= 0 && imgSize < 4) ? QString::fromLatin1(resMap[imgSize]) : QString::number(imgSize);
        m_currentResX = state.resX;
        m_currentResY = state.resY;

        // 图像码率
        QString bitrateStr = QString("%1 Kb/s").arg(state.bitrate);

        // 编码格式映射表
        static const char* codecMap[] = {"H264", "H265"};
        int codec = state.codec;
        QString codecStr = (codec >= 0 && codec < 2) ? QString::fromLatin1(codecMap[codec]) : QString::number(codec);

        // 工作模式映射表
        static const char* wmMap[] = {"关闭AI", "识别", "自动跟踪", "点选跟踪", "波门/框选跟踪"};
        int wm = state.workMode;
        QString wmStr = (wm >= 0 && wm < 5) ? QString::fromUtf8(wmMap[wm]) : QString::number(wm);
        m_previousWorkMode = wm;

        // 显示类型映射表 (PIP = Picture-in-Picture)
        static const char* pipMap[] = {"大图可见光", "红外", "可见光", "融合", "大图红外"};
        int pipRaw = state.currentPipShow;
        int comboIdx = DeviceController::pipShowToComboIndex(pipRaw);
        QString pipStr = (comboIdx >= 0 && comboIdx < 5) ? QString::fromUtf8(pipMap[comboIdx]) : QString::number(pipRaw);

        // 算法模型编码: 高段(传感器)×10 + 低段(识别类型)
        int model = state.model;
        int high = model / 10;
        int low  = model % 10;
        static const char* highMap[] = {"可见光", "红外"};
        static const char* lowMap[]  = {"", "", "人车识别", "船识别", "无人机识别", "飞机直升机识别", "鸟识别"};
        QString modelStr;
        if (high >= 0 && high < 2)
            modelStr = QString::fromUtf8(highMap[high]);
        if (low >= 2 && low <= 6)
            modelStr += QString(" / %1").arg(QString::fromUtf8(lowMap[low]));
        if (modelStr.isEmpty())
            modelStr = QString::number(model);
        m_previousAlgoModel = model;

        m_view->showImageParams(resStr, bitrateStr, codecStr, wmStr, pipStr, modelStr,
                                state.maxVisFL, state.maxIRFL);

        m_currentPipShow = DeviceController::pipShowToComboIndex(state.currentPipShow);
        m_previousDisplayMode = m_currentPipShow;

        // 同步 UI 下拉框到设备当前值，同时抑制信号递归
        m_updatingFromDevice = true;
        // 首次连接时同步算法模型下拉框，后续不再覆盖用户选择
        if (!m_algoModelInitialized) {
            m_currentAlgoModel = model;
            // 高段 = 传感器类型 (0=可见光, 1=红外) → comboAlgoModel1
            m_view->setAlgoModel1Index(high);
            // 低段 = 识别类型 (2-6 → comboAlgoModel2 索引 0-4)
            if (low >= 2 && low <= 6)
                m_view->setAlgoModel2Index(low - 2);
            m_algoModelInitialized = true;
        }
        if (!m_displayModeInitialized) {
            int dIdx = DeviceController::pipShowToComboIndex(state.currentPipShow);
            m_view->setDisplayModeIndex(dIdx);
            m_displayModeInitialized = true;
        }
        // 首次连接时同步工作模式下拉框，后续不再覆盖用户选择
        if (!m_workModeInitialized && wm >= 0) {
            m_view->setWorkModeIndex(wm);
            m_workModeInitialized = true;
        }
        m_updatingFromDevice = false;
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
    MapWidget* map = m_view->mapWidget();
    CameraIntrinsics camInfo;
    CameraConfig& camCfg = m_cfg->cam();
    bool isVis = (m_currentPipShow != 1 && m_currentPipShow != 4);
    camInfo.pixelSizeUm = isVis ? camCfg.visPixelSize : camCfg.irPixelSize;
    camInfo.focalLengthMm = isVis ? camCfg.visMinFocal * m_currentVisZoom : camCfg.irMinFocal * m_currentIrZoom;
    camInfo.resX = isVis ? camCfg.visResX : camCfg.irResX;
    camInfo.resY = isVis ? camCfg.visResY : camCfg.irResY;

    DevicePose devPose;
    devPose.lat = GeoCalculator::parseCoord(m_view->statLatitudeText());
    devPose.lon = GeoCalculator::parseCoord(m_view->statLongitudeText());
    devPose.panDeg = m_view->statPanAngleText().toDouble();

    bool hasObject = doc.contains("Object") && doc.value("Object").isObject();
    QJsonObject objMap;
    if (hasObject) objMap = doc.value("Object").toObject();
    double tilt = m_currentTilt;

    //==========================================================================
    // 识别模式 (WorkMode=1)：显示所有目标，无上报时清空
    //==========================================================================
    if (workMode == 1) {
        if (!hasObject || objMap.isEmpty()) {
            map->clearAllTracks();
            map->clearFov();
            map->updateTargetMarkers(QJsonArray());
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
        map->updateTargetMarkers(targetArr);
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
                        map->appendTrackPoint(lockedId, tLat, tLon, speed);
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
                map->updateTargetMarkers(targetArr);
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
                map->clearAllTracks();
                map->updateTargetMarkers(QJsonArray());
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
                map->updateTargetMarkers(targetArr);
            }
            return;
        }

        // ---- 有 Object 但无 0xB1/0xB2，清空 ----
        map->clearAllTracks();
        map->updateTargetMarkers(QJsonArray());
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

void MainPresenter::updateLensStats()
{
    CameraConfig& cam = m_cfg->cam();
    const double kRad2Deg = 180.0 / 3.14159265358979323846;

    double visFocal = cam.visMinFocal * m_currentVisZoom;
    double irFocal  = cam.irMinFocal * m_currentIrZoom;

    double visHfov = 2.0 * qAtan((cam.visPixelSize * cam.visResX / 1000.0) / (2.0 * visFocal));
    double irHfov = 2.0 * qAtan((cam.irPixelSize * cam.irResX / 1000.0) / (2.0 * irFocal));

    m_view->showLensStats(m_currentVisZoom, visFocal, visHfov * kRad2Deg,
                          m_currentIrZoom, irFocal, irHfov * kRad2Deg);
}

double MainPresenter::calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel)
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
        m_view->setTrackDistance(QString::number(dist, 'f', 1) + QStringLiteral(" m (估算)"));
    return dist;
}

void MainPresenter::updateMapDevicePosition(const DeviceState& state)
{
    MapWidget* map = m_view->mapWidget();
    QString latStr = state.latitudeRaw;
    QString lonStr = state.longitudeRaw;
    double lat = GeoCalculator::parseCoord(latStr);
    double lon = GeoCalculator::parseCoord(lonStr);
    double alt = state.altitude;
    double pan = state.currentPan;
    double tilt = state.currentTilt;

    if (m_cfg->softwarePtzCalibrationEnabled()) {
        pan -= m_cfg->ptzPanOffset();
        while (pan < 0) pan += 360.0;
        while (pan >= 360.0) pan -= 360.0;

        tilt -= m_cfg->ptzTiltOffset();
        while (tilt < -180.0) tilt += 360.0;
        while (tilt > 180.0) tilt -= 360.0;
    }
    double range = state.laserRange;
    bool rangeEstimated = false;
    if (range <= 0) {
        range = m_lastAiDist;
        rangeEstimated = m_lastAiDistEstimated;
    }

    qDebug() << "[MapPos] raw:" << latStr << lonStr << "parsed:" << lat << lon;
    if (lat == 0 && lon == 0) return;

    map->setDevicePosition(lat, lon);

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
    map->setVisFov(lat, lon, pan, tilt, visHfov, visVfov, m_cfg->visFovDistance());
    map->setIrFov(lat, lon, pan, tilt, irHfov, irVfov, m_cfg->irFovDistance());
    map->setDeviceInfo(lat, lon, alt, pan, tilt, visHfov, visVfov, range, rangeEstimated);
}

//============================================================================


void MainPresenter::onDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state) {
    if (deviceId == m_currentDeviceId && state) {
        updateStatusFromState(*state);
    }
}

void MainPresenter::onDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc) {
    if (deviceId == m_currentDeviceId) {
        updateAiInfoFromJson(aiDoc);
    }
}

void MainPresenter::startVideoStream(const QString& url) {
    if (videoStream()) {
        videoStream()->openStream(url);
    }
}






bool MainPresenter::isDeviceConnected() const {
    return tcpClient() && tcpClient()->isConnected();
}

// ============================================================================
// 电机通道初始化/切换（原 MainWindow 构造函数与设置页逻辑）
// ============================================================================
void MainPresenter::initMotorChannel()
{
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        motorController()->openMotorSerial(m_cfg->motorComPort());
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        motorController()->openMotorTcp();
    }
}

void MainPresenter::applyMotorChannel()
{
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        motorController()->openMotorSerial(m_cfg->motorComPort());
        motorController()->closeMotorTcp();
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        motorController()->openMotorTcp();
        motorController()->closeMotorSerial();
    } else {
        motorController()->closeMotorSerial();
        motorController()->closeMotorTcp();
    }
}

// ============================================================================
// 雨刷电机控制
// ============================================================================
void MainPresenter::onWiperStart()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorStart();
}

void MainPresenter::onWiperStop()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorStop();
    QTimer::singleShot(50, this, [this]() {
        motorController()->motorReturnZero();
    });
}

void MainPresenter::onWiperJogLeft()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorJogLeft();
}

void MainPresenter::onWiperJogRight()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorJogRight();
}

void MainPresenter::onWiperJogStop()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorStop();
}

void MainPresenter::onWiperZeroCalib()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorZeroCalib();
}

void MainPresenter::onWiperMode()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorToggleMode();
    QTimer::singleShot(500, this, [this]() {
        motorController()->motorCheckMode();
    });
}

void MainPresenter::onWiperSilent()
{
    if (!m_view->requireMotorReady()) return;
    motorController()->motorToggleSilentMode();
}

void MainPresenter::onWiperCurrentSet()
{
    if (!m_view->requireMotorReady()) return;
    int ma = m_view->wiperCurrentMa();
    motorController()->motorSetCurrent(ma);
    m_view->showStatusMessage(
        QString("正在下发并固化电机电流: %1 mA").arg(ma), 3000);
}

void MainPresenter::checkMotorMode()
{
    motorController()->motorCheckMode();
}

// ============================================================================
// 预置位与复位
// ============================================================================
void MainPresenter::on_btnCallPreset_clicked()
{
    if (!m_view->requireConnected()) return;
    motorController()->callPreset(m_view->presetValue());
}

void MainPresenter::on_btnSetPreset_clicked()
{
    if (!m_view->requireConnected()) return;
    motorController()->setPreset(m_view->presetValue());
}

void MainPresenter::on_btnDelPreset_clicked()
{
    if (!m_view->requireConnected()) return;
    motorController()->delPreset(m_view->presetValue());
}

void MainPresenter::on_btnPtzReset_clicked()
{
    if (!m_view->requireConnected()) return;
    motorController()->callPreset(0);
}

// ============================================================================
// 附加功能开关（下发设备指令 + 持久化配置 + ACK 帧类型记录）
// ============================================================================
void MainPresenter::onCheckDigitalZoomToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setDigitalZoomChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetDigitalZoom;
    motorController()->setDigitalZoom(checked);
    m_cfg->setDigitalZoomEnabled(checked);
    m_cfg->save();
}

void MainPresenter::onCheckAutoZoomToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setAutoZoomChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetAlgoModel;
    motorController()->setAutoZoom(checked);
    m_cfg->setAutoZoomEnabled(checked);
    m_cfg->save();
}

void MainPresenter::onCheckCaptureUploadToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setCaptureUploadChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetCaptureState;
    motorController()->setCaptureUpload(checked);
    m_cfg->setCaptureUploadEnabled(checked);
    m_cfg->save();
}

void MainPresenter::onCheckPosResetToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setPosResetChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetPosReset;
    motorController()->posReset(checked);
    m_cfg->setPosResetEnabled(checked);
    m_cfg->save();
}

// ============================================================================
// 框选/点选跟踪
// ============================================================================
void MainPresenter::onVideoSelection(const QString& deviceId, int cx, int cy, int pw, int ph)
{
    int wm = m_view->workModeIndex();
    if (wm != 3 && wm != 4) {
        m_view->showStatusMessage(
            QString::fromUtf8("仅在点选跟踪或框选跟踪模式下支持框选"), 3000);
        return;
    }

    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (!ctx || !ctx->tcpClient() || !ctx->tcpClient()->isConnected()) {
        m_view->showStatusMessage(
            QString::fromUtf8("设备未连接: %1").arg(deviceId), 3000);
        return;
    }
    DeviceController* controller = ctx->motorController();

    if (wm == 3) {
        m_view->showStatusMessage(
            QString::fromUtf8("点选跟踪: 像素中心(%1,%2)").arg(cx).arg(cy));
        controller->setPointTrack(cx, cy);
    } else {
        m_view->showStatusMessage(
            QString::fromUtf8("框选跟踪: 像素中心(%1,%2) 宽%3高%4")
                .arg(cx).arg(cy).arg(pw).arg(ph));
        controller->setBoxTrack(cx, cy, pw, ph);
    }
}

// ============================================================================
// 工作模式 / 算法模型 / 显示模式
// ============================================================================
void MainPresenter::onComboWorkModeChanged(int index)
{
    // 非点选/框选跟踪模式时禁止鼠标框选（本地 UI 状态，不涉及设备指令）
    if (auto vw = m_view->videoWidget(m_currentDeviceId))
        vw->setSelectionEnabled(index == 3 || index == 4);

    if (m_updatingFromDevice) return;

    if (!m_view->requireConnected()) {
        m_updatingFromDevice = true;
        m_view->setWorkModeIndex(m_previousWorkMode);
        m_updatingFromDevice = false;
        return;
    }
    m_previousWorkMode = index;
    motorController()->setWorkMode(index);
    motorController()->queryImageParams();
}

void MainPresenter::sendAlgoModel(int model)
{
    if (m_updatingFromDevice) return;
    if (!m_view->requireConnected()) return;
    m_currentAlgoModel = model;
    m_previousAlgoModel = model;
    motorController()->setAlgoModel(model);
    motorController()->queryImageParams();
}

void MainPresenter::onComboDisplayModeChanged(int index)
{
    if (!m_view->requireConnected()) {
        m_view->setDisplayModeIndex(m_previousDisplayMode);
        return;
    }
    if (m_updatingFromDevice) return;
    // 根据显示模式自动切换算法模型：0/2/3→可见光模型，1/4→红外模型
    // 直接下发不触发 queryImageParams，避免设备返回旧数据覆盖显示模式
    {
        int algoIdx = (index == 1 || index == 4) ? 1 : 0;
        if ((m_currentAlgoModel / 10) != algoIdx) {
            int low = m_view->algoModel2Index();
            int model = algoIdx * 10 + (low >= 0 ? low + 2 : 0);
            m_currentAlgoModel = model;
            m_view->setAlgoModel1Index(algoIdx);
            motorController()->setAlgoModel(model);
        }
    }
    // 延后发送显示模式，避免与 setAlgoModel 间隔过近被设备忽略
    QTimer::singleShot(150, this, [this]() {
        if (isDeviceConnected()) {
            int idx = m_view->displayModeIndex();
            motorController()->setDisplayMode(idx);
        }
    });
}

// ============================================================================
// ACK 应答处理（原 MainWindow::onAckReceived）
// ACK 状态码: 0=正常, 1=包不完整, 2=协议内容错误
// SetDigitalZoom/SetCaptureState/SetPosReset 设备固定回 1，按成功处理
// ============================================================================
void MainPresenter::showAck(quint8 statusCode)
{
    if (statusCode == 0) {
        m_view->showStatusMessage(QString::fromUtf8("[ACK] 指令执行成功"), 3000);
        return;
    }
    if (statusCode == 1) {
        if (m_lastAckFrameType == FrameType::SetDigitalZoom
            || m_lastAckFrameType == FrameType::SetCaptureState
            || m_lastAckFrameType == FrameType::SetPosReset) {
            m_view->showStatusMessage(QString::fromUtf8("[ACK] 指令执行成功"), 3000);
            return;
        }
        m_view->showStatusMessage(QString::fromUtf8("[ACK] 包不完整"), 3000);
        return;
    }
    QString msg;
    switch (statusCode) {
    case 2: msg = QString::fromUtf8("协议内容错误"); break;
    default: msg = QString::fromUtf8("未知状态码: %1").arg(statusCode);
    }
    m_view->showStatusMessage(QString::fromUtf8("[ACK] %1").arg(msg), 3000);
}
