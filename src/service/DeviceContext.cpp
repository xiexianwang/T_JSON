#include "DeviceContext.h"
#include "core/EventBus.h"
#include "core/JsonFrameParser.h"
#include "core/GeoCalculator.h"

DeviceContext::DeviceContext(const QString& deviceId, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_deviceId(deviceId)
    , m_cfg(cfg)
    , m_state(std::make_shared<DeviceState>())
{
    // 初始化底层驱动组件
    m_tcp = new TJsonClient(this);
    m_motor = new DeviceController(m_tcp, m_cfg, this);
    m_video = new RtspThread(this);
    m_ptz = new PtzForwarder(this);
    setupTimers();

    // --- 转发 DeviceController 信号（业务层收口，View 经 Presenter 订阅） ---
    connect(m_motor, &DeviceController::commandSent, this, &DeviceContext::commandSent);
    connect(m_motor, &DeviceController::motorModeResult, this, &DeviceContext::motorModeResult);
    connect(m_motor, &DeviceController::motorSilentResult, this, &DeviceContext::motorSilentResult);
    connect(m_motor, &DeviceController::motorSerialError, this, &DeviceContext::motorSerialError);
    connect(m_motor, &DeviceController::motorTcpError, this, &DeviceContext::motorTcpError);

    // --- 拦截设备 JSON 帧，解析后更新 DeviceState ---
    connect(m_tcp, &TJsonClient::jsonReceived, this, [this](const QJsonObject& doc) {
        if (!isActive()) return;
        QString controlType = doc.value("ControlType").toString();

        if (controlType == "ZoomInfo") {
            auto zoom = ZoomInfoData::parse(doc);
            m_state->currentVisZoom = zoom.visZoom;
            m_state->currentIrZoom = zoom.irZoom;
            m_state->camShowMode = zoom.camShowMode;
            m_state->currentPan = zoom.pan;
            m_state->currentTilt = zoom.tilt;
            m_state->latitudeRaw = zoom.latitude;
            m_state->longitudeRaw = zoom.longitude;
            m_state->latitude = GeoCalculator::parseCoord(zoom.latitude);
            m_state->longitude = GeoCalculator::parseCoord(zoom.longitude);
            m_state->altitude = zoom.height;
            m_state->laserRange = zoom.laserRange;
            EventBus::instance()->postDeviceStateUpdated(m_deviceId, m_state, m_sessionGeneration);
        }
        else if (controlType == "ImageSetting") {
            auto img = ImageSettingData::parse(doc);
            m_state->imgSize = img.imgSize;
            m_state->bitrate = img.bitrate;
            m_state->codec = img.codec;
            m_state->workMode = img.workMode;
            m_state->currentPipShow = img.pipShow;
            m_state->model = img.model;
            m_state->maxVisFL = img.maxVisFL;
            m_state->maxIRFL = img.maxIRFL;

            // 分辨率表
            static const int resTab[][2] = {{1920,1080},{1280,720},{704,576},{2566,1520}};
            if (img.imgSize >= 0 && img.imgSize < 4) {
                m_state->resX = resTab[img.imgSize][0];
                m_state->resY = resTab[img.imgSize][1];
            }

            EventBus::instance()->postDeviceStateUpdated(m_deviceId, m_state, m_sessionGeneration);
        }
        else if (controlType == "AIInfo") {
            auto ai = AiInfoData::parse(doc);
            m_state->lastAiInfoTime = QDateTime::currentDateTime();
            m_state->aiWorkMode = ai.workMode;
            m_state->aiObjectCount = ai.objectCount;
            m_state->aiTargets.clear();
            for (const auto& t : ai.targets) {
                AiTargetItem item;
                item.id = t.id;
                item.cls = t.cls;
                item.distance = t.distance;
                item.hasPoints = t.hasPoints;
                item.left = t.left;
                item.top = t.top;
                item.right = t.right;
                item.bottom = t.bottom;
                m_state->aiTargets.append(item);
            }
            EventBus::instance()->postDeviceAiInfoUpdated(m_deviceId, doc, m_sessionGeneration);
        }
    });
}

DeviceContext::~DeviceContext()
{
    // shutdown() 应已在 DeviceManager::removeAllDevices() 中被调用
    // 此处兜底：仅在异常路径（未被显式关闭）时执行
    if (m_lifecycleState == State::Active) {
        (void)shutdown();
    }
}

// ============================================================================
// 连接与状态
// ============================================================================
void DeviceContext::connectDevice(const QString& ip, quint16 port)
{
    if (m_lifecycleState != State::Active) return;
    if (m_cfg) {
        m_tcp->connectToDevice(ip, port);
    }
}

void DeviceContext::disconnectDevice()
{
    (void)shutdown();
}

DeviceContext::ShutdownResult DeviceContext::shutdown()
{
    ShutdownResult result;
    if (m_lifecycleState == State::ShuttingDown || m_lifecycleState == State::Stopped) {
        result.alreadyStopped = true;
        result.rtspStopped = !m_video->isRunning();
        result.ptzStopped = true;
        result.motorStopped = !m_motor->isMotorSerialOpen() && !m_motor->isMotorTcpOpen();
        result.networkStopped = !m_tcp->isConnected();
        return result;
    }
    m_lifecycleState = State::ShuttingDown;

    // 停止业务定时器
    m_sysParamTimer->stop();
    m_aiCleanupTimer->stop();

    // 停止 PTZ 转发
    m_ptz->stop();
    result.ptzStopped = true;

    // 关闭电机 TCP 和串口
    m_motor->closeMotorTcp();
    m_motor->closeMotorSerial();
    result.motorStopped = !m_motor->isMotorSerialOpen() && !m_motor->isMotorTcpOpen();

    // 停止 RTSP 并等待线程退出
    m_video->closeStream();
    if (m_video->isRunning()) {
        qWarning() << "DeviceContext::shutdown() - RTSP thread timeout for device:" << m_deviceId;
        result.error = QStringLiteral("RTSP 线程未在关闭超时内退出");
    }
    result.rtspStopped = !m_video->isRunning();

    // 断开 TJsonClient 并取消自动重连
    m_tcp->disconnectDevice();
    result.networkStopped = !m_tcp->isConnected();

    m_lifecycleState = State::Stopped;
    return result;
}

void DeviceContext::disconnectNetwork()
{
    if (!isActive()) return;
    m_tcp->disconnectDevice();
}

bool DeviceContext::isConnected() const
{
    return m_tcp->isConnected();
}

// ============================================================================
// 视频流
// ============================================================================
void DeviceContext::startVideo(const QString& url)
{
    if (m_lifecycleState != State::Active) return;
    m_video->openStream(url);
}

void DeviceContext::stopVideo()
{
    if (!isActive()) return;
    m_video->closeStream();
}

bool DeviceContext::isVideoRunning() const
{
    return m_video->isRunning();
}

// ============================================================================
// 云台控制 (Pelco-D)
// ============================================================================
void DeviceContext::ptzMove(PtzDir dir)
{
    if (!isActive()) return;
    m_motor->ptzMove(dir);
}

void DeviceContext::ptzStop()
{
    if (!isActive()) return;
    m_motor->ptzStop();
}

void DeviceContext::ptzMoveTo(double pan, double tilt)
{
    if (!isActive()) return;
    m_motor->ptzMoveTo(pan, tilt);
}

void DeviceContext::ptzSetZero()
{
    if (!isActive()) return;
    m_motor->ptzSetZero();
}

// ============================================================================
// 镜头控制
// ============================================================================
void DeviceContext::lensZoomIn(int target)
{
    if (!isActive()) return;
    m_motor->lensZoomIn(target);
}

void DeviceContext::lensZoomOut(int target)
{
    if (!isActive()) return;
    m_motor->lensZoomOut(target);
}

void DeviceContext::lensFocusIn(int target)
{
    if (!isActive()) return;
    m_motor->lensFocusIn(target);
}

void DeviceContext::lensFocusOut(int target)
{
    if (!isActive()) return;
    m_motor->lensFocusOut(target);
}

void DeviceContext::lensStop()
{
    if (!isActive()) return;
    m_motor->lensStop();
}

// ============================================================================
// 图像参数 / 工作模式 / 算法 / 显示
// ============================================================================
void DeviceContext::queryImageParams()
{
    if (!isActive()) return;
    m_motor->queryImageParams();
}

void DeviceContext::setWorkMode(int mode)
{
    if (!isActive()) return;
    m_motor->setWorkMode(mode);
}

void DeviceContext::setAlgoModel(int model)
{
    if (!isActive()) return;
    m_motor->setAlgoModel(model);
}

void DeviceContext::setDisplayMode(int mode)
{
    if (!isActive()) return;
    m_motor->setDisplayMode(mode);
}

void DeviceContext::setLocation(const QString& lat, const QString& lon)
{
    if (!isActive()) return;
    m_motor->setLocation(lat, lon);
}

// ============================================================================
// 附加功能开关
// ============================================================================
void DeviceContext::setDigitalZoom(bool enable)
{
    if (!isActive()) return;
    m_motor->setDigitalZoom(enable);
}

void DeviceContext::setAutoZoom(bool enable)
{
    if (!isActive()) return;
    m_motor->setAutoZoom(enable);
}

void DeviceContext::setCaptureUpload(bool enable)
{
    if (!isActive()) return;
    m_motor->setCaptureUpload(enable);
}

void DeviceContext::posReset(bool enable)
{
    if (!isActive()) return;
    m_motor->posReset(enable);
}

// ============================================================================
// 框选/点选跟踪
// ============================================================================
void DeviceContext::setPointTrack(int centerX, int centerY)
{
    if (!isActive()) return;
    m_motor->setPointTrack(centerX, centerY);
}

void DeviceContext::setBoxTrack(int centerX, int centerY, int width, int height)
{
    if (!isActive()) return;
    m_motor->setBoxTrack(centerX, centerY, width, height);
}

// ============================================================================
// 预置位
// ============================================================================
void DeviceContext::setPreset(int preset)
{
    if (!isActive()) return;
    m_motor->setPreset(preset);
}

void DeviceContext::callPreset(int preset)
{
    if (!isActive()) return;
    m_motor->callPreset(preset);
}

void DeviceContext::delPreset(int preset)
{
    if (!isActive()) return;
    m_motor->delPreset(preset);
}

// ============================================================================
// 电机通道管理
// ============================================================================
bool DeviceContext::openMotorSerial(const QString& portName)
{
    if (!isActive()) return false;
    return m_motor->openMotorSerial(portName);
}

void DeviceContext::closeMotorSerial()
{
    if (!isActive()) return;
    m_motor->closeMotorSerial();
}

bool DeviceContext::isMotorSerialOpen() const
{
    return m_motor->isMotorSerialOpen();
}

void DeviceContext::openMotorTcp()
{
    if (!isActive()) return;
    m_motor->openMotorTcp();
}

void DeviceContext::closeMotorTcp()
{
    if (!isActive()) return;
    m_motor->closeMotorTcp();
}

bool DeviceContext::isMotorTcpOpen() const
{
    return m_motor->isMotorTcpOpen();
}

// ============================================================================
// 雨刷电机控制
// ============================================================================
void DeviceContext::motorStart()
{
    if (!isActive()) return;
    m_motor->motorStart();
}

void DeviceContext::motorStop()
{
    if (!isActive()) return;
    m_motor->motorStop();
}

void DeviceContext::motorWiperStop()
{
    if (!isActive()) return;
    m_motor->motorWiperStop();
}

void DeviceContext::motorReturnZero()
{
    if (!isActive()) return;
    m_motor->motorReturnZero();
}

void DeviceContext::motorJogLeft()
{
    if (!isActive()) return;
    m_motor->motorJogLeft();
}

void DeviceContext::motorJogRight()
{
    if (!isActive()) return;
    m_motor->motorJogRight();
}

void DeviceContext::motorZeroCalib()
{
    if (!isActive()) return;
    m_motor->motorZeroCalib();
}

void DeviceContext::motorCheckMode()
{
    if (!isActive()) return;
    m_motor->motorCheckMode();
}

void DeviceContext::motorToggleMode()
{
    if (!isActive()) return;
    m_motor->motorToggleMode();
}

void DeviceContext::motorToggleSilentMode()
{
    if (!isActive()) return;
    m_motor->motorToggleSilentMode();
}

void DeviceContext::motorSetCurrent(int ma)
{
    if (!isActive()) return;
    m_motor->motorSetCurrent(ma);
}

// ============================================================================
// PTZ 转发服务
// ============================================================================
void DeviceContext::startPtzForwarder(const QString& ptzIp, quint16 ptzPort, quint16 mockServerPort)
{
    if (!isActive()) return;
    m_ptz->start(ptzIp, ptzPort, mockServerPort);
}

void DeviceContext::setPtzOffsets(double panOffset, double tiltOffset)
{
    if (!isActive()) return;
    m_ptz->setOffsets(panOffset, tiltOffset);
}

void DeviceContext::flushZeroPosition()
{
    if (!isActive()) return;
    m_ptz->flushZeroPosition();
}


void DeviceContext::setupTimers()
{
    m_sysParamTimer = new QTimer(this);
    m_sysParamTimer->setInterval(500);
    connect(m_sysParamTimer, &QTimer::timeout, this, [this]() {
        if (m_lifecycleState != State::Active) return;
        if (m_tcp->isConnected()) {
            m_motor->queryImageParams();
        }
    });

    m_aiCleanupTimer = new QTimer(this);
    m_aiCleanupTimer->setInterval(1000);
    connect(m_aiCleanupTimer, &QTimer::timeout, this, [this]() {
        if (m_lifecycleState != State::Active) return;
        if (m_state->lastAiInfoTime.isValid() && m_state->lastAiInfoTime.msecsTo(QDateTime::currentDateTime()) >= 2000) {
            m_state->lastAiInfoTime = QDateTime();
            m_state->aiObjectCount = 0;
            m_state->aiTargets.clear();
            EventBus::instance()->postDeviceAiTimeout(m_deviceId, m_sessionGeneration);
        }
    });

    connect(m_tcp, &TJsonClient::deviceConnected, this, [this]() {
        if (!isActive()) return;
        m_sysParamTimer->start();
        m_aiCleanupTimer->start();
        EventBus::instance()->postDeviceConnected(m_deviceId, m_sessionGeneration);
    });
    connect(m_video, &RtspThread::frameReady, this, [this](const QImage& frame) {
        if (!isActive()) return;
        EventBus::instance()->postDeviceFrameReady(m_deviceId, frame, m_sessionGeneration);
    });
    connect(m_video, &RtspThread::streamOpened, this, [this]() {
        if (!isActive()) return;
        EventBus::instance()->postRtspOpened(m_deviceId, m_sessionGeneration);
    });
    connect(m_video, &RtspThread::streamError, this, [this](const QString& msg) {
        if (!isActive()) return;
        EventBus::instance()->postRtspError(m_deviceId, msg, m_sessionGeneration);
    });

    connect(m_tcp, &TJsonClient::deviceDisconnected, this, [this]() {
        if (!isActive()) return;
        m_sysParamTimer->stop();
        m_aiCleanupTimer->stop();
        EventBus::instance()->postDeviceDisconnected(m_deviceId, m_sessionGeneration);
    });
    connect(m_tcp, &TJsonClient::errorOccurred, this, [this](const QString& errorMsg) {
        if (!isActive()) return;
        EventBus::instance()->postDeviceError(m_deviceId, errorMsg, m_sessionGeneration);
    });
    connect(m_tcp, &TJsonClient::imageSnapped, this, [this](const QByteArray& jpegData, const QRect& location) {
        if (!isActive()) return;
        EventBus::instance()->postImageSnapped(m_deviceId, jpegData, location, m_sessionGeneration);
    });
    connect(m_tcp, &TJsonClient::ackReceived, this, [this](quint8 statusCode) {
        if (!isActive()) return;
        EventBus::instance()->postAckReceived(m_deviceId, statusCode, m_sessionGeneration);
    });
    connect(m_tcp, &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        if (!isActive()) return;
        EventBus::instance()->postDeviceReconnecting(m_deviceId, attempt, maxRetries, m_sessionGeneration);
    });
    connect(m_tcp, &TJsonClient::reconnectFailed, this, [this]() {
        if (!isActive()) return;
        EventBus::instance()->postDeviceReconnectFailed(m_deviceId, m_sessionGeneration);
    });
}
