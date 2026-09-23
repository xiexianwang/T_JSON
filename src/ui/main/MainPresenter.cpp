#include <QJsonArray>
#include "MainPresenter.h"
#include "PresenterDeviceService.h"
#include "PresenterMotorService.h"
#include "PresenterMapService.h"
#include "DeviceStateService.h"
#include "PresenterStateViewService.h"
#include "PresenterMediaService.h"
#include "PresenterAiViewService.h"
#include "DeviceControlService.h"
#include "IMainView.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "core/GeoCalculator.h"
#include <QMessageBox>
#include <QTimer>

MainPresenter::MainPresenter(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_cfg(cfg)
{
    DeviceManager::instance().init(m_cfg);

    m_deviceService = new PresenterDeviceService(view, cfg, this);
    m_motorService = new PresenterMotorService(view, cfg, this);
    m_mapService = new PresenterMapService(view, cfg, this);
    m_stateService = new DeviceStateService(this);
    m_stateViewService = new PresenterStateViewService(view, cfg, m_mapService, this);
    m_mediaService = new PresenterMediaService(view, this);
    m_aiViewService = new PresenterAiViewService(view, cfg, m_mapService, this);
    m_controlService = new DeviceControlService(cfg, this);

    // 注入电机服务到设备服务（用于连接时初始化电机/转台）
    m_deviceService->setMotorService(m_motorService);

    // 不再预创建默认设备：设备上下文在设备树激活设备时按需创建。

    // DeviceService EventBus → MainPresenter 信号连接
    connect(m_deviceService, &PresenterDeviceService::deviceConnected, this, &MainPresenter::onDeviceConnected);
    connect(m_deviceService, &PresenterDeviceService::deviceDisconnected, this, &MainPresenter::onDeviceDisconnected);
    connect(m_deviceService, &PresenterDeviceService::deviceFrameReady, this, [this](const QString& deviceId, const QImage& frame) {
        if (m_view) m_view->setVideoFrame(deviceId, frame);
    });
    connect(m_deviceService, &PresenterDeviceService::videoSlotUnavailable, this, [this](const QString& deviceId) {
        Q_UNUSED(deviceId);
        if (m_view)
            m_view->showStatusMessage(
                QString::fromUtf8("视频宫格已满，请切换更大布局后再连接"), 5000);
    });
    connect(m_deviceService, &PresenterDeviceService::deviceStateUpdated, this, &MainPresenter::onDeviceStateUpdated);
    connect(m_deviceService, &PresenterDeviceService::deviceAiInfoUpdated, this, &MainPresenter::onDeviceAiInfoUpdated);
    connect(m_deviceService, &PresenterDeviceService::deviceAiTimeout, this, &MainPresenter::onDeviceAiTimeout);
    connect(m_deviceService, &PresenterDeviceService::deviceError, this, [this](const QString& deviceId, const QString& errorMsg) {
        if (m_view) m_view->onErrorOccurred(errorMsg);
    });
    connect(m_deviceService, &PresenterDeviceService::rtspOpened, this, [this](const QString& deviceId) {
        if (m_view) m_view->onRtspOpened(deviceId);
    });
    connect(m_deviceService, &PresenterDeviceService::rtspError, this, [this](const QString& deviceId, const QString& msg) {
        if (m_view) m_view->onRtspError(deviceId, msg);
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
    connect(m_deviceService, &PresenterDeviceService::motorModeChanged, this,
            [this](const QString& deviceId, bool value) {
        if (deviceId == currentDeviceId()) emit motorModeChanged(value);
    });
    connect(m_deviceService, &PresenterDeviceService::motorSerialError, this,
            [this](const QString& deviceId, const QString& msg) {
        if (deviceId == currentDeviceId()) emit motorSerialErrorOccurred(msg);
    });
    connect(m_deviceService, &PresenterDeviceService::motorTcpError, this,
            [this](const QString& deviceId, const QString& msg) {
        if (deviceId == currentDeviceId()) emit motorTcpErrorOccurred(msg);
    });
    connect(m_deviceService, &PresenterDeviceService::motorSilentChanged, this,
            [this](const QString& deviceId, bool value) {
        if (deviceId == currentDeviceId()) emit motorSilentChanged(value);
    });
    connect(m_deviceService, &PresenterDeviceService::motorCurrentChanged, this,
            [this](const QString& deviceId, int run, int hold, int delay) {
        if (deviceId == currentDeviceId()) emit motorCurrentChanged(run, hold, delay);
    });
    connect(m_deviceService, &PresenterDeviceService::commandSent, this,
            [this](const QString& deviceId, const QString& type, const QByteArray& data) {
        if (deviceId == currentDeviceId()) emit commandSentToLog(type, data);
    });

    connect(m_deviceService, &PresenterDeviceService::rtspStatsChanged, this,
            [this](const QString& deviceId, const RtspThread::Stats& stats) {
        if (deviceId != currentDeviceId()) return;
        emit rtspStatsChanged(deviceId, stats);
        if (m_view) m_view->onRtspStats(deviceId, stats);
    });

    // 设备切换后重置状态缓存
    connect(m_deviceService, &PresenterDeviceService::deviceSwitched, this, &MainPresenter::onDeviceSwitched);

    // 注：currentDeviceChanged → 设备树标记由 View 自行订阅，
    // 避免把设备树这一具体控件细节塞进 IMainView 窄接口。

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
    DeviceContext* ctx = currentDevice();
    if (ctx) ctx->connectDevice(ip, port);
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

void MainPresenter::initPtzForwarderForDevice(const QString& deviceId)
{
    m_motorService->initPtzForwarder(deviceId);
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

QString MainPresenter::motorProtocol() const
{
    DeviceContext* ctx = currentDevice();
    return ctx ? ctx->deviceConfig().motorProtocol : m_cfg->motorProtocol();
}

QString MainPresenter::motorCommandChannel() const
{
    DeviceContext* ctx = currentDevice();
    return ctx ? ctx->deviceConfig().motorCommandChannel : m_cfg->motorCommandChannel();
}

bool MainPresenter::isVideoStreamRunning() const
{
    return m_mediaService->isStreamRunning(currentDevice());
}

void MainPresenter::closeVideoStream()
{
    m_mediaService->closeStream(currentDevice());
}

void MainPresenter::on_btnPtzMoveTo_clicked()
{
    if (!m_view->requireConnected()) return;

    bool panOk = false;
    bool tiltOk = false;
    double pan = m_view->targetPanText().toDouble(&panOk);
    double tilt = m_view->targetTiltText().toDouble(&tiltOk);

    if (panOk && tiltOk) {
        m_controlService->ptzMoveTo(currentDeviceId(), pan, tilt);
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

    bool lonOk = false;
    bool latOk = false;
    double targetLon = GeoCalculator::parseCoord(lonStr, &lonOk);
    double targetLat = GeoCalculator::parseCoord(latStr, &latOk);
    // 高度可留空（按 0 处理），但填了就必须是合法数值
    bool altOk = true;
    double targetAlt = 0.0;
    if (!altStr.isEmpty()) targetAlt = altStr.toDouble(&altOk);
    if (!lonOk || !latOk || !altOk) {
        QMessageBox::warning(m_view->asWidget(), "输入错误",
                             "请输入有效的目标经度、纬度与高度。");
        return;
    }

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

    m_controlService->ptzMoveTo(currentDeviceId(), pan, tilt);
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

            m_controlService->setPtzOffsets(currentDeviceId(), newPanOffset, newTiltOffset);

            m_view->showDeviceState(QString(), QString(), QString(), "0.0°", "0.0°");
            m_view->showStatusMessage("零点标定(软件偏置)已保存", 3000);
        } else {
            m_controlService->ptzSetZero(currentDeviceId());
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

    bool latOk = false;
    bool lonOk = false;
    double latNum = GeoCalculator::parseCoord(latStr, &latOk);
    double lonNum = GeoCalculator::parseCoord(lonStr, &lonOk);
    QString altStr = m_view->setHeightText().trimmed();
    bool altOk = true;
    double altNum = 0.0;
    if (!altStr.isEmpty()) altNum = altStr.toDouble(&altOk);
    if (!latOk || !lonOk || !altOk) {
        QMessageBox::warning(m_view->asWidget(), QString::fromUtf8("输入错误"),
                             QString::fromUtf8("请输入有效的经纬度/高度数值"));
        return;
    }

    if (!m_view->requireConnected()) return;

    if (!altStr.isEmpty()) {
        m_deviceHeight = altNum;
        m_view->showDeviceState(QString(), QString(),
                                QString::number(m_deviceHeight, 'f', 1) + QStringLiteral(" m"),
                                QString(), QString());
    }

    QString strictLat = QString::asprintf("%.7f%s", qAbs(latNum), latNum >= 0 ? "N" : "S");
    QString strictLon = QString::asprintf("%.7f%s", qAbs(lonNum), lonNum >= 0 ? "E" : "W");

    m_controlService->setLocation(currentDeviceId(), strictLat, strictLon);
    m_view->showStatusMessage(QString::fromUtf8("已下发经纬度"), 3000);
}

void MainPresenter::on_btnGetImageParams_clicked()
{
    if (!m_view->requireConnected()) return;
    m_controlService->queryImageParams(currentDeviceId());
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

void MainPresenter::onDeviceActivated(const QString& deviceId, const DeviceEntry& entry)
{
    m_deviceService->activateDevice(deviceId, entry);
}

void MainPresenter::onDeviceToggleConnect(const QString& deviceId, const DeviceEntry& entry)
{
    m_deviceService->toggleDeviceConnect(deviceId, entry);
}

void MainPresenter::selectDevice(const QString& deviceId)
{
    m_deviceService->focusDevice(deviceId);
}

void MainPresenter::onDeviceRemoved(const QString& deviceId)
{
    m_stateService->deviceRemoved(deviceId);
    m_mapService->resetDevice(deviceId);
    m_deviceService->removeDevice(deviceId);
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
    updateAiInfoFromJson(deviceId, aiDoc);
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
    if (snapshot.hasAi) updateAiInfoFromJson(deviceId, snapshot.aiInfo);
    refreshSwitchView(deviceId);
    emit currentDeviceChanged(deviceId);
}

void MainPresenter::refreshSwitchView(const QString& deviceId)
{
    if (!m_view) return;
    const DeviceConfig cfg = m_deviceService->currentDevice() ? m_deviceService->currentDevice()->deviceConfig() : DeviceConfig();
    m_view->setDigitalZoomChecked(cfg.digitalZoom);
    m_view->setAutoZoomChecked(cfg.autoZoom);
    m_view->setCaptureUploadChecked(cfg.captureUpload);
    m_view->setPosResetChecked(cfg.posReset);
}

void MainPresenter::updateAiInfoFromJson(const QString& deviceId, const QJsonObject& aiDoc)
{
    const DeviceSnapshot snapshot = m_stateService->snapshot(deviceId);
    const DeviceState state = snapshot.statePtr ? *snapshot.statePtr : DeviceState();
    m_aiViewService->updateAiInfo(deviceId, aiDoc, state, currentStateViewCache());
}

void MainPresenter::onDeviceConnected(const QString& deviceId)
{
    if (m_view) m_view->onDeviceConnected(deviceId);
}

void MainPresenter::onDeviceDisconnected(const QString& deviceId)
{
    if (m_view) m_view->onDeviceDisconnected(deviceId);
}

void MainPresenter::resetDeviceStateCache()
{
    m_cache = {};
    m_deviceHeight = 0;
}

StateViewCache MainPresenter::currentStateViewCache() const
{
    return m_cache;
}

void MainPresenter::applyStateViewCache(const StateViewCache& cache)
{
    m_cache = cache;
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

void MainPresenter::applyMotorChannelForDevice(const QString& deviceId)
{
    m_motorService->applyMotorChannel(deviceId);
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
    int run = m_view->wiperRunCurrent();
    int hold = m_view->wiperHoldCurrent();
    int delay = m_view->wiperHoldDelay();
    m_motorService->onWiperCurrentSet(m_deviceService->currentDeviceId(), run, hold, delay);

    DeviceContext* ctx = currentDevice();
    const QString proto = ctx ? ctx->deviceConfig().motorProtocol : QString();
    if (proto == "STM32-TCP-V4.0") {
        m_view->showStatusMessage(
            QString("正在下发电流: 运行%1 保持%2 延迟%3").arg(run).arg(hold).arg(delay), 3000);
    } else {
        m_view->showStatusMessage(QString("正在下发电流: 运行%1 mA").arg(run), 3000);
    }
}

void MainPresenter::checkMotorMode()
{
    m_motorService->checkMotorMode(m_deviceService->currentDeviceId());
}

void MainPresenter::readMotorCurrent()
{
    m_motorService->readMotorCurrent(m_deviceService->currentDeviceId());
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
    m_controlService->setDigitalZoom(currentDeviceId(), checked);
    saveDeviceSwitchConfig([](DeviceConfig& c, bool v) { c.digitalZoom = v; }, checked);
}

void MainPresenter::onCheckAutoZoomToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setAutoZoomChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetAlgoModel;
    m_controlService->setAutoZoom(currentDeviceId(), checked);
    saveDeviceSwitchConfig([](DeviceConfig& c, bool v) { c.autoZoom = v; }, checked);
}

void MainPresenter::onCheckCaptureUploadToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setCaptureUploadChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetCaptureState;
    m_controlService->setCaptureUpload(currentDeviceId(), checked);
    saveDeviceSwitchConfig([](DeviceConfig& c, bool v) { c.captureUpload = v; }, checked);
}

void MainPresenter::onCheckPosResetToggled(bool checked)
{
    if (!m_view->requireConnected()) {
        m_view->setPosResetChecked(!checked);
        return;
    }
    m_lastAckFrameType = FrameType::SetPosReset;
    m_controlService->setPosReset(currentDeviceId(), checked);
    saveDeviceSwitchConfig([](DeviceConfig& c, bool v) { c.posReset = v; }, checked);
}

void MainPresenter::saveDeviceSwitchConfig(const std::function<void(DeviceConfig&, bool)>& setter, bool value)
{
    DeviceContext* ctx = currentDevice();
    if (!ctx) return;
    setter(ctx->deviceConfig(), value);
    emit deviceConfigChanged(currentDeviceId(), ctx->deviceConfig());
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

    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId); ctx && !ctx->isConnected()) {
        m_view->showStatusMessage(
            QString::fromUtf8("设备未连接: %1").arg(deviceId), 3000);
        return;
    }

    if (wm == 3) {
        m_view->showStatusMessage(
            QString::fromUtf8("点选跟踪: 像素中心(%1,%2)").arg(cx).arg(cy));
        m_controlService->setPointTrack(deviceId, cx, cy);
    } else {
        m_view->showStatusMessage(
            QString::fromUtf8("框选跟踪: 像素中心(%1,%2) 宽%3高%4")
                .arg(cx).arg(cy).arg(pw).arg(ph));
        m_controlService->setBoxTrack(deviceId, cx, cy, pw, ph);
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
        m_view->setWorkModeIndex(m_cache.previousWorkMode);
        m_updatingFromDevice = false;
        return;
    }
    m_cache.previousWorkMode = index;
    m_controlService->setWorkMode(currentDeviceId(), index);
    m_controlService->queryImageParams(currentDeviceId());
}

void MainPresenter::sendAlgoModel(int model)
{
    if (m_updatingFromDevice) return;
    if (!m_view->requireConnected()) return;
    m_cache.currentAlgoModel = model;
    m_cache.previousAlgoModel = model;
    m_controlService->setAlgoModel(currentDeviceId(), model);
    m_controlService->queryImageParams(currentDeviceId());
}

void MainPresenter::onComboDisplayModeChanged(int index)
{
    if (!m_view->requireConnected()) {
        m_view->setDisplayModeIndex(m_cache.previousDisplayMode);
        return;
    }
    if (m_updatingFromDevice) return;
    {
        int algoIdx = (index == 1 || index == 4) ? 1 : 0;
        if ((m_cache.currentAlgoModel / 10) != algoIdx) {
            int low = m_view->algoModel2Index();
            int model = algoIdx * 10 + (low >= 0 ? low + 2 : 0);
            m_cache.currentAlgoModel = model;
            m_view->setAlgoModel1Index(algoIdx);
            m_controlService->setAlgoModel(currentDeviceId(), model);
        }
    }
    QTimer::singleShot(150, this, [this]() {
        if (isDeviceConnected()) {
            int idx = m_view->displayModeIndex();
            m_controlService->setDisplayMode(currentDeviceId(), idx);
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
