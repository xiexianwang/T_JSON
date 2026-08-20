#include <QJsonArray>
#include "MainPresenter.h"
#include "PresenterDeviceService.h"
#include "PresenterMotorService.h"
#include "PresenterMapService.h"
#include "DeviceStateService.h"
#include "PresenterStateViewService.h"
#include "IMainView.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "core/GeoCalculator.h"
#include <QMessageBox>
#include <QVariant>
#include <QTimer>
#include <QtMath>

MainPresenter::MainPresenter(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_cfg(cfg)
{
    DeviceManager::instance()->init(m_cfg);

    m_deviceService = new PresenterDeviceService(this, view, cfg, this);
    m_motorService = new PresenterMotorService(view, cfg, this);
    m_mapService = new PresenterMapService(view, cfg, this);
    m_stateService = new DeviceStateService(this);
    m_stateViewService = new PresenterStateViewService(view, cfg, m_mapService, this);

    // DeviceService 初始化默认设备
    DeviceManager::instance()->addDevice(m_deviceService->currentDeviceId());

    // DeviceService EventBus → MainPresenter 信号连接
    connect(m_deviceService, &PresenterDeviceService::deviceConnected, this, &MainPresenter::onDeviceConnected);
    connect(m_deviceService, &PresenterDeviceService::deviceDisconnected, this, &MainPresenter::onDeviceDisconnected);
    connect(m_deviceService, &PresenterDeviceService::deviceFrameReady, this, [this](const QString& deviceId, const QImage& frame) {
        if (m_view) m_view->setVideoFrame(deviceId, frame);
    });
    connect(m_deviceService, &PresenterDeviceService::deviceStateUpdated, this, &MainPresenter::onDeviceStateUpdated);
    connect(m_deviceService, &PresenterDeviceService::deviceAiInfoUpdated, this, &MainPresenter::onDeviceAiInfoUpdated);
    connect(m_deviceService, &PresenterDeviceService::deviceAiTimeout, this, &MainPresenter::onDeviceAiTimeout);
    connect(m_deviceService, &PresenterDeviceService::deviceError, this, [this](const QString& deviceId, const QString& errorMsg) {
        if (m_view) m_view->onErrorOccurred(errorMsg);
    });
    connect(m_deviceService, &PresenterDeviceService::rtspOpened, this, [this](const QString& deviceId) {
        Q_UNUSED(deviceId);
        if (m_view) m_view->onRtspOpened();
    });
    connect(m_deviceService, &PresenterDeviceService::rtspError, this, [this](const QString& deviceId, const QString& msg) {
        Q_UNUSED(deviceId);
        if (m_view) m_view->onRtspError(msg);
    });
    connect(m_deviceService, &PresenterDeviceService::imageSnapped, this, [this](const QString& deviceId, const QByteArray& jpegData, const QRect& location) {
        Q_UNUSED(deviceId);
        if (m_view) m_view->onImageSnapped(jpegData, location);
    });
    connect(m_deviceService, &PresenterDeviceService::ackReceived, this, [this](const QString& deviceId, quint8 statusCode) {
        Q_UNUSED(deviceId);
        showAck(statusCode);
    });
    connect(m_deviceService, &PresenterDeviceService::deviceReconnecting, this, [this](const QString& deviceId, int attempt, int maxRetries) {
        Q_UNUSED(deviceId);
        if (m_view) m_view->onDeviceReconnecting(attempt, maxRetries);
    });
    connect(m_deviceService, &PresenterDeviceService::deviceReconnectFailed, this, [this](const QString& deviceId) {
        Q_UNUSED(deviceId);
        if (m_view) m_view->onDeviceReconnectFailed();
    });

    // 设备切换后重置状态缓存
    connect(m_deviceService, &PresenterDeviceService::deviceSwitched, this, &MainPresenter::onDeviceSwitched);

    // DeviceService 内部初始化 EventBus 连接
    m_deviceService->setupEventBus();
}

MainPresenter::~MainPresenter()
{
}

DeviceContext* MainPresenter::currentDevice() const
{
    return m_deviceService->currentDevice();
}

QString MainPresenter::currentDeviceId() const
{
    return m_deviceService->currentDeviceId();
}

void MainPresenter::connectToDevice(const QString& ip, quint16 port)
{
    m_deviceService->connectToDevice(m_deviceService->currentDeviceId(), ip, port);
}

void MainPresenter::disconnectDevice()
{
    m_deviceService->disconnectDevice(m_deviceService->currentDeviceId());
}

void MainPresenter::ptzMove(int direction)
{
    m_motorService->ptzMove(m_deviceService->currentDeviceId(), direction);
}

void MainPresenter::ptzStop()
{
    m_motorService->ptzStop(m_deviceService->currentDeviceId());
}

void MainPresenter::lensMove(int op)
{
    m_motorService->lensMove(m_deviceService->currentDeviceId(), op);
}

void MainPresenter::lensStop()
{
    m_motorService->lensStop(m_deviceService->currentDeviceId());
}

void MainPresenter::initPtzForwarder()
{
    m_motorService->initPtzForwarder(m_deviceService->currentDeviceId());
}

bool MainPresenter::isMotorSerialOpen() const
{
    DeviceContext* ctx = currentDevice();
    return ctx && ctx->isMotorSerialOpen();
}

bool MainPresenter::isMotorTcpOpen() const
{
    DeviceContext* ctx = currentDevice();
    return ctx && ctx->isMotorTcpOpen();
}

bool MainPresenter::isVideoStreamRunning() const
{
    DeviceContext* ctx = currentDevice();
    return ctx && ctx->isVideoRunning();
}

void MainPresenter::closeVideoStream()
{
    if (DeviceContext* ctx = currentDevice()) ctx->stopVideo();
}

// --- Extracted from MainWindow ---
void MainPresenter::on_btnConnect_clicked()
{
    if (isDeviceConnected()) {
        m_deviceService->disconnectDevice(m_deviceService->currentDeviceId());
    } else {
        if (!m_cfg->turntableIpEnabled()) {
             m_view->showStatusMessage(QString::fromUtf8("转台IP连接已禁用"), 3000);
             return;
        }
        QString ip = m_view->ipText();
        m_deviceService->connectToDevice(m_deviceService->currentDeviceId(), ip, m_cfg->deviceTcpPort());
        m_view->setConnectButton(QString::fromUtf8("连接中..."), false, QString(), true);
    }
}

void MainPresenter::on_btnCancelConnect_clicked()
{
    m_deviceService->disconnectDevice(m_deviceService->currentDeviceId());
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
    if (DeviceContext* ctx = currentDevice()) ctx->startVideo(url);
    m_view->setVideoConnectButton(QString::fromUtf8("连接中..."), false);
    m_view->showStatusMessage(QString::fromUtf8("正在连接 RTSP 视频流..."));
}

void MainPresenter::on_btnVideoDisconnect_clicked()
{
    if (DeviceContext* ctx = currentDevice()) ctx->stopVideo();
    m_view->repaintVideoGrid();
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
        if (DeviceContext* ctx = currentDevice()) ctx->ptzMoveTo(pan, tilt);
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

    if (DeviceContext* ctx = currentDevice()) ctx->ptzMoveTo(pan, tilt);
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

            if (DeviceContext* ctx = currentDevice()) {
                ctx->setPtzOffsets(newPanOffset, newTiltOffset);
                ctx->flushZeroPosition();
            }

            m_view->showDeviceState(-1, QString(), QString(), QString(), "0.0°", "0.0°");
            m_view->showStatusMessage("零点标定(软件偏置)已保存", 3000);
        } else {
            if (DeviceContext* ctx = currentDevice()) ctx->ptzSetZero();
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

    if (DeviceContext* ctx = currentDevice()) ctx->setLocation(strictLat, strictLon);
    m_view->showStatusMessage(QString::fromUtf8("已下发经纬度"), 3000);
}

void MainPresenter::on_btnGetImageParams_clicked()
{
    if (!m_view->requireConnected()) return;
    if (DeviceContext* ctx = currentDevice()) ctx->queryImageParams();
    m_view->showStatusMessage(QString::fromUtf8("已发送参数查询请求"), 3000);
}



void MainPresenter::onDeviceAiTimeout(const QString& deviceId)
{
    Q_UNUSED(deviceId);
    m_view->setIdentifyCount(QString::fromUtf8("目标总数: 0"));
    m_view->clearIdentifyTable();

    m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
    m_view->setTrackPos(QString());
    m_view->setTrackMissDistance(QString());
    m_view->setTrackDistance(QString());

    m_view->mapClearAllTracks();
    m_view->mapUpdateTargetMarkers(QJsonArray());
    m_view->mapClearFov();
}

void MainPresenter::onDeviceDoubleClicked(const QString& name, const QString& ip, const QString& rtspUrl)
{
    QString deviceId = QString("dev_%1").arg(ip);
    m_deviceService->switchToDevice(deviceId, ip, rtspUrl);
}

void MainPresenter::onDeviceToggleConnect(const QString& ip)
{
    m_deviceService->toggleDeviceConnect(ip);
}

void MainPresenter::onDeviceRemoved(const QString& ip)
{
    const QString deviceId = QString("dev_%1").arg(ip);
    m_stateService->deviceRemoved(deviceId);
    m_mapService->resetDevice(deviceId);
    m_deviceService->removeDevice(ip);
}

void MainPresenter::updateAiInfoFromJson(const QJsonObject& doc)
{
    CameraConfig& cam = m_cfg->cam();

    //==========================================================================
    // AIInfo - AI 识别与跟踪结果帧
    //==========================================================================
    {
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
        if ((workMode == 1) || (workMode >= 2 && workMode <= 4)) {
            DeviceSnapshot snapshot = m_stateService->snapshot(m_deviceService->currentDeviceId());
            DeviceState state = snapshot.statePtr ? *snapshot.statePtr : DeviceState();
            state.aiWorkMode = workMode;
            m_mapService->updateAiInfo(m_deviceService->currentDeviceId(), doc, state);
        }

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

double MainPresenter::calcVisualDistance(const QJsonObject& obj, int cls, bool updateTrackLabel)
{
    DeviceSnapshot snapshot = m_stateService->snapshot(m_deviceService->currentDeviceId());
    DeviceState state = snapshot.statePtr ? *snapshot.statePtr : DeviceState();
    return m_mapService->calculateVisualDistance(m_deviceService->currentDeviceId(), obj, cls,
                                                 state, updateTrackLabel);
}

//============================================================================


void MainPresenter::onDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state) {
    m_stateService->updateState(deviceId, state);
    if (state) {
        m_updatingFromDevice = true;
        applyStateViewCache(m_stateViewService->updateStatusFromState(deviceId, *state,
                                                                       currentStateViewCache()));
        m_updatingFromDevice = false;
    }
}

void MainPresenter::onDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc) {
    m_stateService->updateAi(deviceId, aiDoc);
    updateAiInfoFromJson(aiDoc);
}

void MainPresenter::startVideoStream(const QString& url) {
    if (DeviceContext* ctx = currentDevice()) ctx->startVideo(url);
}

bool MainPresenter::isDeviceConnected() const {
    DeviceContext* ctx = currentDevice();
    return ctx && ctx->isConnected();
}

void MainPresenter::onDeviceSwitched()
{
    const QString deviceId = m_deviceService->currentDeviceId();
    const DeviceSnapshot snapshot = m_stateService->snapshot(deviceId);
    resetDeviceStateCache();
    m_mapService->resetDevice(deviceId);
    m_updatingFromDevice = true;
    if (snapshot.hasState && snapshot.statePtr) {
        applyStateViewCache(m_stateViewService->updateStatusFromState(deviceId, *snapshot.statePtr,
                                                                        currentStateViewCache()));
    } else {
        DeviceState emptyState;
        applyStateViewCache(m_stateViewService->updateStatusFromState(deviceId, emptyState,
                                                                        currentStateViewCache()));
    }
    m_updatingFromDevice = false;
    if (snapshot.hasAi) updateAiInfoFromJson(snapshot.aiInfo);
}

void MainPresenter::onDeviceConnected()
{
    if (m_view) m_view->onDeviceConnected();
}

void MainPresenter::onDeviceDisconnected()
{
    if (m_view) m_view->onDeviceDisconnected();
}

void MainPresenter::resetDeviceStateCache()
{
    m_currentVisZoom = 1.0;
    m_currentIrZoom = 1.0;
    m_currentTilt = 0.0;
    m_currentPipShow = 0;
    m_previousWorkMode = 0;
    m_workModeInitialized = false;
    m_displayModeInitialized = false;
    m_algoModelInitialized = false;
    m_previousAlgoModel = 0;
    m_currentAlgoModel = 0;
    m_previousDisplayMode = 0;
    m_currentResX = 2688;
    m_currentResY = 1520;
    m_lastAiDist = 0;
    m_lastAiDistEstimated = false;
    m_deviceHeight = 0;
    m_rtspEverOpened = false;
}

StateViewCache MainPresenter::currentStateViewCache() const
{
    return {m_currentVisZoom, m_currentIrZoom, m_currentTilt, m_currentPipShow,
            m_previousWorkMode, m_workModeInitialized, m_displayModeInitialized,
            m_algoModelInitialized, m_previousAlgoModel, m_currentAlgoModel,
            m_previousDisplayMode, m_currentResX, m_currentResY};
}

void MainPresenter::applyStateViewCache(const StateViewCache& cache)
{
    m_currentVisZoom = cache.currentVisZoom;
    m_currentIrZoom = cache.currentIrZoom;
    m_currentTilt = cache.currentTilt;
    m_currentPipShow = cache.currentPipShow;
    m_previousWorkMode = cache.previousWorkMode;
    m_workModeInitialized = cache.workModeInitialized;
    m_displayModeInitialized = cache.displayModeInitialized;
    m_algoModelInitialized = cache.algoModelInitialized;
    m_previousAlgoModel = cache.previousAlgoModel;
    m_currentAlgoModel = cache.currentAlgoModel;
    m_previousDisplayMode = cache.previousDisplayMode;
    m_currentResX = cache.currentResX;
    m_currentResY = cache.currentResY;
}

// ============================================================================
// 电机通道初始化/切换（原 MainWindow 构造函数与设置页逻辑）
// ============================================================================
void MainPresenter::initMotorChannel()
{
    m_motorService->initMotorChannel(m_deviceService->currentDeviceId());
}

void MainPresenter::applyMotorChannel()
{
    m_motorService->applyMotorChannel(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperStart()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperStart(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperStop()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperStop(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperJogLeft()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperJogLeft(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperJogRight()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperJogRight(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperJogStop()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperJogStop(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperZeroCalib()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperZeroCalib(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperMode()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperMode(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperSilent()
{
    if (!m_view->requireMotorReady()) return;
    m_motorService->onWiperSilent(m_deviceService->currentDeviceId());
}

void MainPresenter::onWiperCurrentSet()
{
    if (!m_view->requireMotorReady()) return;
    int ma = m_view->wiperCurrentMa();
    m_motorService->onWiperCurrentSet(m_deviceService->currentDeviceId(), ma);
    m_view->showStatusMessage(
        QString("正在下发并固化电机电流: %1 mA").arg(ma), 3000);
}

void MainPresenter::checkMotorMode()
{
    m_motorService->checkMotorMode(m_deviceService->currentDeviceId());
}

void MainPresenter::on_btnCallPreset_clicked()
{
    if (!m_view->requireConnected()) return;
    m_motorService->callPreset(m_deviceService->currentDeviceId(), m_view->presetValue());
}

void MainPresenter::on_btnSetPreset_clicked()
{
    if (!m_view->requireConnected()) return;
    m_motorService->setPreset(m_deviceService->currentDeviceId(), m_view->presetValue());
}

void MainPresenter::on_btnDelPreset_clicked()
{
    if (!m_view->requireConnected()) return;
    m_motorService->delPreset(m_deviceService->currentDeviceId(), m_view->presetValue());
}

void MainPresenter::on_btnPtzReset_clicked()
{
    if (!m_view->requireConnected()) return;
    m_motorService->callPreset(m_deviceService->currentDeviceId(), 0);
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
    if (DeviceContext* ctx = currentDevice()) ctx->setDigitalZoom(checked);
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
    if (DeviceContext* ctx = currentDevice()) ctx->setAutoZoom(checked);
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
    if (DeviceContext* ctx = currentDevice()) ctx->setCaptureUpload(checked);
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
    if (DeviceContext* ctx = currentDevice()) ctx->posReset(checked);
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
    if (!ctx || !ctx->isConnected()) {
        m_view->showStatusMessage(
            QString::fromUtf8("设备未连接: %1").arg(deviceId), 3000);
        return;
    }

    if (wm == 3) {
        m_view->showStatusMessage(
            QString::fromUtf8("点选跟踪: 像素中心(%1,%2)").arg(cx).arg(cy));
        ctx->setPointTrack(cx, cy);
    } else {
        m_view->showStatusMessage(
            QString::fromUtf8("框选跟踪: 像素中心(%1,%2) 宽%3高%4")
                .arg(cx).arg(cy).arg(pw).arg(ph));
        ctx->setBoxTrack(cx, cy, pw, ph);
    }
}

// ============================================================================
// 工作模式 / 算法模型 / 显示模式
// ============================================================================
void MainPresenter::onComboWorkModeChanged(int index)
{
    m_view->setVideoSelectionEnabled(m_deviceService->currentDeviceId(), index == 3 || index == 4);

    if (m_updatingFromDevice) return;

    if (!m_view->requireConnected()) {
        m_updatingFromDevice = true;
        m_view->setWorkModeIndex(m_previousWorkMode);
        m_updatingFromDevice = false;
        return;
    }
    m_previousWorkMode = index;
    if (DeviceContext* ctx = currentDevice()) {
        ctx->setWorkMode(index);
        ctx->queryImageParams();
    }
}

void MainPresenter::sendAlgoModel(int model)
{
    if (m_updatingFromDevice) return;
    if (!m_view->requireConnected()) return;
    m_currentAlgoModel = model;
    m_previousAlgoModel = model;
    if (DeviceContext* ctx = currentDevice()) {
        ctx->setAlgoModel(model);
        ctx->queryImageParams();
    }
}

void MainPresenter::onComboDisplayModeChanged(int index)
{
    if (!m_view->requireConnected()) {
        m_view->setDisplayModeIndex(m_previousDisplayMode);
        return;
    }
    if (m_updatingFromDevice) return;
    {
        int algoIdx = (index == 1 || index == 4) ? 1 : 0;
        if ((m_currentAlgoModel / 10) != algoIdx) {
            int low = m_view->algoModel2Index();
            int model = algoIdx * 10 + (low >= 0 ? low + 2 : 0);
            m_currentAlgoModel = model;
            m_view->setAlgoModel1Index(algoIdx);
            if (DeviceContext* ctx = currentDevice()) ctx->setAlgoModel(model);
        }
    }
    QTimer::singleShot(150, this, [this]() {
        if (isDeviceConnected()) {
            int idx = m_view->displayModeIndex();
            if (DeviceContext* ctx = currentDevice()) ctx->setDisplayMode(idx);
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
