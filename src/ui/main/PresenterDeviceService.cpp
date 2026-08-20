#include "PresenterDeviceService.h"
#include "MainPresenter.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "ui/main/IMainView.h"
#include "core/EventBus.h"
#include "infrastructure/configmanager.h"

PresenterDeviceService::PresenterDeviceService(MainPresenter* parentPresenter, IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent), m_presenter(parentPresenter), m_view(view), m_cfg(cfg)
    , m_currentDeviceId("default_device")
{
}

PresenterDeviceService::~PresenterDeviceService()
{
}

// ============================================================================
// 设备连接/断开
// ============================================================================
void PresenterDeviceService::connectToDevice(const QString& deviceId, const QString& ip, quint16 port)
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (!ctx) return;
    ctx->connectDevice(ip, port);
}

void PresenterDeviceService::disconnectDevice(const QString& deviceId)
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (!ctx) return;
    ctx->disconnectNetwork();
}

void PresenterDeviceService::toggleDeviceConnect(const QString& ip)
{
    QString deviceId = QString("dev_%1").arg(ip);
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (!ctx) return;

    if (ctx->isConnected()) {
        ctx->disconnectNetwork();
    } else {
        ctx->connectDevice(ip, m_cfg->deviceTcpPort());
    }
}

bool PresenterDeviceService::isDeviceConnected(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    return ctx && ctx->isConnected();
}

// ============================================================================
// 视频流
// ============================================================================
void PresenterDeviceService::startVideoStream(const QString& deviceId, const QString& url)
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (ctx) ctx->startVideo(url);
}

void PresenterDeviceService::stopVideo(const QString& deviceId)
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    if (ctx) ctx->stopVideo();
}

bool PresenterDeviceService::isVideoRunning(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    return ctx && ctx->isVideoRunning();
}

// ============================================================================
// 当前设备管理
// ============================================================================
DeviceContext* PresenterDeviceService::currentDevice() const
{
    if (m_currentDeviceId.isEmpty()) return nullptr;
    return DeviceManager::instance()->getDevice(m_currentDeviceId);
}

bool PresenterDeviceService::isMotorSerialOpen(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    return ctx && ctx->isMotorSerialOpen();
}

bool PresenterDeviceService::isMotorTcpOpen(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
    return ctx && ctx->isMotorTcpOpen();
}

// ============================================================================
// 设备切换
// ============================================================================
void PresenterDeviceService::switchToDevice(const QString& newDeviceId, const QString& ip, const QString& rtspUrl)
{
    if (newDeviceId == m_currentDeviceId) return;

    // 1. 解绑旧设备电机信号
    disconnectDeviceSignals();

    // 2. 清理旧设备 UI 状态（地图/跟踪/识别）
    m_view->mapClearAllTracks();
    m_view->mapUpdateTargetMarkers(QJsonArray());
    m_view->mapClearFov();
    m_view->clearIdentifyTable();
    m_view->setIdentifyCount(QString());
    m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
    m_view->setTrackPos(QString());
    m_view->setTrackDistance(QString());
    m_view->setTrackMissDistance(QString());

    // 3. 切换当前设备 ID
    m_currentDeviceId = newDeviceId;

    // 4. 获取或创建设备上下文
    DeviceContext* ctx = DeviceManager::instance()->getDevice(newDeviceId);
    if (!ctx) {
        ctx = DeviceManager::instance()->addDevice(newDeviceId);
    }

    // 5. 绑定新设备电机信号
    connectDeviceSignals(ctx);

    // 6. 绑定视频控件
    m_view->setVideoFrame(newDeviceId, QImage());

    // 7. 通知 MainPresenter 刷新仪表盘
    emit deviceSwitched();

    // 8. 连接设备
    ctx->connectDevice(ip, m_cfg->deviceTcpPort());

    // 9. 自动开始 RTSP
    if (!rtspUrl.isEmpty()) {
        ctx->startVideo(rtspUrl);
    }
}

void PresenterDeviceService::removeDevice(const QString& ip)
{
    QString deviceId = QString("dev_%1").arg(ip);
    bool wasCurrent = (m_currentDeviceId == deviceId);

    if (wasCurrent) {
        disconnectDeviceSignals();
        m_view->clearVideoFrame(deviceId);
        m_view->mapClearAllTracks();
        m_view->mapUpdateTargetMarkers(QJsonArray());
        m_view->mapClearFov();
        m_view->clearIdentifyTable();
    }

    DeviceManager::instance()->removeDevice(deviceId);

    if (wasCurrent) {
        QList<QString> remaining = DeviceManager::instance()->getAllDeviceIds();
        if (!remaining.isEmpty()) {
            QString nextId = remaining.first();
            m_currentDeviceId = nextId;
            connectDeviceSignals(DeviceManager::instance()->getDevice(nextId));
            m_view->setVideoFrame(nextId, QImage());
            emit deviceSwitched();
        } else {
            m_currentDeviceId.clear();
            emit deviceSwitched();
            m_view->showStatusMessage(QString::fromUtf8("所有设备已断开"), 3000);
        }
    }
}

// ============================================================================
// 电机信号连接管理
// ============================================================================
void PresenterDeviceService::connectDeviceSignals(DeviceContext* ctx)
{
    if (!ctx) return;
    m_motorModeConn = connect(ctx, &DeviceContext::motorModeResult, m_presenter, &MainPresenter::motorModeChanged);
    m_motorErrorConn = connect(ctx, &DeviceContext::motorSerialError, m_presenter, &MainPresenter::motorSerialErrorOccurred);
    m_motorTcpErrorConn = connect(ctx, &DeviceContext::motorTcpError, m_presenter, &MainPresenter::motorTcpErrorOccurred);
    m_motorSilentConn = connect(ctx, &DeviceContext::motorSilentResult, m_presenter, &MainPresenter::motorSilentChanged);
    m_commandLogConn = connect(ctx, &DeviceContext::commandSent, m_presenter, &MainPresenter::commandSentToLog);
}

void PresenterDeviceService::disconnectDeviceSignals()
{
    disconnect(m_motorModeConn);
    disconnect(m_motorErrorConn);
    disconnect(m_motorTcpErrorConn);
    disconnect(m_motorSilentConn);
    disconnect(m_commandLogConn);
}

// ============================================================================
// EventBus 连接
// ============================================================================
void PresenterDeviceService::setupEventBus()
{
    EventBus* bus = EventBus::instance();

    // 帧就绪
    connect(bus, &EventBus::sigDeviceFrameReady, this, &PresenterDeviceService::deviceFrameReady);

    // 设备连接/断开
    connect(bus, &EventBus::sigDeviceConnected, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) {
            // 初始化参数下发
            DeviceContext* ctx = DeviceManager::instance()->getDevice(deviceId);
            if (ctx) {
                ctx->queryImageParams();
                ctx->setDigitalZoom(m_cfg->digitalZoomEnabled());
                ctx->setAutoZoom(m_cfg->autoZoomEnabled());
                ctx->setCaptureUpload(m_cfg->captureUploadEnabled());
                ctx->posReset(m_cfg->posResetEnabled());
            }
            emit deviceConnected();
        }
    });

    connect(bus, &EventBus::sigDeviceDisconnected, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) emit deviceDisconnected();
    });

    // RTSP
    connect(bus, &EventBus::sigRtspOpened, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) emit rtspOpened(deviceId);
    });
    connect(bus, &EventBus::sigRtspError, this, [this](const QString& deviceId, const QString& msg) {
        if (deviceId == m_currentDeviceId) emit rtspError(deviceId, msg);
    });

    // 设备状态/AI
    connect(bus, &EventBus::sigDeviceStateUpdated, this, [this](const QString& deviceId, std::shared_ptr<DeviceState> state) {
        if (deviceId == m_currentDeviceId && state) emit deviceStateUpdated(deviceId, state);
    });
    connect(bus, &EventBus::sigDeviceAiInfoUpdated, this, [this](const QString& deviceId, const QJsonObject& aiDoc) {
        if (deviceId == m_currentDeviceId) emit deviceAiInfoUpdated(deviceId, aiDoc);
    });
    connect(bus, &EventBus::sigDeviceAiTimeout, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) emit deviceAiTimeout(deviceId);
    });

    // 错误/抓拍/ACK/重连
    connect(bus, &EventBus::sigDeviceError, this, [this](const QString& deviceId, const QString& errorMsg) {
        if (deviceId == m_currentDeviceId) emit deviceError(deviceId, errorMsg);
    });
    connect(bus, &EventBus::sigImageSnapped, this, [this](const QString& deviceId, const QByteArray& jpegData, const QRect& location) {
        if (deviceId == m_currentDeviceId) emit imageSnapped(deviceId, jpegData, location);
    });
    connect(bus, &EventBus::sigAckReceived, this, [this](const QString& deviceId, quint8 statusCode) {
        if (deviceId == m_currentDeviceId) emit ackReceived(deviceId, statusCode);
    });
    connect(bus, &EventBus::sigDeviceReconnecting, this, [this](const QString& deviceId, int attempt, int maxRetries) {
        if (deviceId == m_currentDeviceId) emit deviceReconnecting(deviceId, attempt, maxRetries);
    });
    connect(bus, &EventBus::sigDeviceReconnectFailed, this, [this](const QString& deviceId) {
        if (deviceId == m_currentDeviceId) emit deviceReconnectFailed(deviceId);
    });
}
