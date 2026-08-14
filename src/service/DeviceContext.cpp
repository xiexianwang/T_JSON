#include "DeviceContext.h"
#include "core/EventBus.h"
#include "core/JsonFrameParser.h"

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

    // --- 拦截设备 JSON 帧，解析后更新 DeviceState ---
    connect(m_tcp, &TJsonClient::jsonReceived, this, [this](const QJsonObject& doc) {
        QString controlType = doc.value("ControlType").toString();

        if (controlType == "ZoomInfo") {
            auto zoom = ZoomInfoData::parse(doc);
            m_state->currentVisZoom = zoom.visZoom;
            m_state->currentIrZoom = zoom.irZoom;
            m_state->camShowMode = zoom.camShowMode;
            m_state->currentPan = zoom.pan;
            m_state->currentTilt = zoom.tilt;
            m_state->latitude = zoom.latitude.toDouble();
            m_state->longitude = zoom.longitude.toDouble();
            m_state->altitude = zoom.height;
            m_state->laserRange = zoom.laserRange;
            EventBus::instance()->postDeviceStateUpdated(m_deviceId, m_state);
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

            EventBus::instance()->postDeviceStateUpdated(m_deviceId, m_state);
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
            EventBus::instance()->postDeviceAiInfoUpdated(m_deviceId, doc);
        }
    });
}

DeviceContext::~DeviceContext()
{
    stopConnection();
    
}

void DeviceContext::startConnection(const QString& ip, quint16 port)
{
    if (m_cfg) {
        m_tcp->connectToDevice(ip, port);
    }
}

void DeviceContext::stopConnection()
{
    m_sysParamTimer->stop();
    m_aiCleanupTimer->stop();

    m_ptz->stop();
    m_motor->closeMotorTcp();
    m_motor->closeMotorSerial();
    m_video->closeStream();

    // 即使当前已经断开，也必须调用主动断开以取消自动重连定时器。
    m_tcp->disconnectDevice();
}


void DeviceContext::setupTimers()
{
    m_sysParamTimer = new QTimer(this);
    m_sysParamTimer->setInterval(500);
    connect(m_sysParamTimer, &QTimer::timeout, this, [this]() {
        if (m_tcp->isConnected()) {
            m_motor->queryImageParams();
        }
    });

    m_aiCleanupTimer = new QTimer(this);
    m_aiCleanupTimer->setInterval(1000);
    connect(m_aiCleanupTimer, &QTimer::timeout, this, [this]() {
        if (m_state->lastAiInfoTime.isValid() && m_state->lastAiInfoTime.msecsTo(QDateTime::currentDateTime()) >= 2000) {
            m_state->lastAiInfoTime = QDateTime();
            EventBus::instance()->postDeviceAiTimeout(m_deviceId);
        }
    });

    connect(m_tcp, &TJsonClient::deviceConnected, this, [this]() {
        m_sysParamTimer->start();
        m_aiCleanupTimer->start();
        EventBus::instance()->postDeviceConnected(m_deviceId);
    });
    connect(m_video, &RtspThread::frameReady, this, [this](const QImage& frame) {
        EventBus::instance()->postDeviceFrameReady(m_deviceId, frame);
    });
    connect(m_video, &RtspThread::streamOpened, this, [this]() {
        EventBus::instance()->postRtspOpened(m_deviceId);
    });
    connect(m_video, &RtspThread::streamError, this, [this](const QString& msg) {
        EventBus::instance()->postRtspError(m_deviceId, msg);
    });

    connect(m_tcp, &TJsonClient::deviceDisconnected, this, [this]() {
        m_sysParamTimer->stop();
        m_aiCleanupTimer->stop();
        EventBus::instance()->postDeviceDisconnected(m_deviceId);
    });
    connect(m_tcp, &TJsonClient::errorOccurred, this, [this](const QString& errorMsg) {
        EventBus::instance()->postDeviceError(m_deviceId, errorMsg);
    });
    connect(m_tcp, &TJsonClient::imageSnapped, this, [this](const QByteArray& jpegData, const QRect& location) {
        EventBus::instance()->postImageSnapped(m_deviceId, jpegData, location);
    });
    connect(m_tcp, &TJsonClient::ackReceived, this, [this](quint8 statusCode) {
        EventBus::instance()->postAckReceived(m_deviceId, statusCode);
    });
    connect(m_tcp, &TJsonClient::reconnecting, this, [this](int attempt, int maxRetries) {
        EventBus::instance()->postDeviceReconnecting(m_deviceId, attempt, maxRetries);
    });
    connect(m_tcp, &TJsonClient::reconnectFailed, this, [this]() {
        EventBus::instance()->postDeviceReconnectFailed(m_deviceId);
    });
    connect(m_tcp, &TJsonClient::jsonReceived, this, [this](const QJsonObject& doc) {
        EventBus::instance()->postJsonReceived(m_deviceId, doc);
    });
}
