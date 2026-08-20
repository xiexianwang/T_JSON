#include "PresenterDeviceService.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "ui/main/IMainView.h"
#include "core/EventBus.h"
#include "infrastructure/configmanager.h"
#include "ui/main/DeviceSessionService.h"

PresenterDeviceService::PresenterDeviceService(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent), m_view(view), m_cfg(cfg)
    , m_session(new DeviceSessionService(cfg, this))
    , m_currentDeviceId("default_device")
{
    m_session->ensureDevice(m_currentDeviceId);
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
// 当前设备管理
// ============================================================================
DeviceContext* PresenterDeviceService::currentDevice() const
{
    return m_session->currentDevice();
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
    if (newDeviceId == m_session->currentDeviceId()) return;

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
    m_session->ensureDevice(newDeviceId);
    m_session->selectDevice(newDeviceId);
    m_currentDeviceId = m_session->currentDeviceId();

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
    bool wasCurrent = (m_session->currentDeviceId() == deviceId);

    if (wasCurrent) {
        disconnectDeviceSignals();
        m_view->clearVideoFrame(deviceId);
        m_view->mapClearAllTracks();
        m_view->mapUpdateTargetMarkers(QJsonArray());
        m_view->mapClearFov();
        m_view->clearIdentifyTable();
    }

    if (!m_session->removeDevice(deviceId)) return;

    if (wasCurrent) {
        QList<QString> remaining = DeviceManager::instance()->getAllDeviceIds();
        if (!remaining.isEmpty()) {
            QString nextId = remaining.first();
            m_session->selectDevice(nextId);
            m_currentDeviceId = m_session->currentDeviceId();
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
    const QString deviceId = ctx->deviceId();
    m_motorModeConn = connect(ctx, &DeviceContext::motorModeResult, this,
                              [this, deviceId](bool value) { emit motorModeChanged(deviceId, value); });
    m_motorErrorConn = connect(ctx, &DeviceContext::motorSerialError, this,
                               [this, deviceId](const QString& msg) { emit motorSerialError(deviceId, msg); });
    m_motorTcpErrorConn = connect(ctx, &DeviceContext::motorTcpError, this,
                                  [this, deviceId](const QString& msg) { emit motorTcpError(deviceId, msg); });
    m_motorSilentConn = connect(ctx, &DeviceContext::motorSilentResult, this,
                                [this, deviceId](bool value) { emit motorSilentChanged(deviceId, value); });
    m_commandLogConn = connect(ctx, &DeviceContext::commandSent, this,
                               [this, deviceId](const QString& type, const QByteArray& data) {
        emit commandSent(deviceId, type, data);
    });
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
    connect(bus, &EventBus::sigDeviceFrameReady, this,
            [this](const QString& deviceId, const QImage& frame, quint64 generation) {
        if (acceptsEvent(deviceId, generation)) emit deviceFrameReady(deviceId, frame);
    });

    // 设备连接/断开
    connect(bus, &EventBus::sigDeviceConnected, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) {
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

    connect(bus, &EventBus::sigDeviceDisconnected, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit deviceDisconnected();
    });

    // RTSP
    connect(bus, &EventBus::sigRtspOpened, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit rtspOpened(deviceId);
    });
    connect(bus, &EventBus::sigRtspError, this, [this](const QString& deviceId, const QString& msg, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit rtspError(deviceId, msg);
    });

    // 设备状态/AI
    connect(bus, &EventBus::sigDeviceStateUpdated, this, [this](const QString& deviceId, std::shared_ptr<DeviceState> state, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId && state) emit deviceStateUpdated(deviceId, state);
    });
    connect(bus, &EventBus::sigDeviceAiInfoUpdated, this, [this](const QString& deviceId, const QJsonObject& aiDoc, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit deviceAiInfoUpdated(deviceId, aiDoc);
    });
    connect(bus, &EventBus::sigDeviceAiTimeout, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit deviceAiTimeout(deviceId);
    });

    // 错误/抓拍/ACK/重连
    connect(bus, &EventBus::sigDeviceError, this, [this](const QString& deviceId, const QString& errorMsg, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit deviceError(deviceId, errorMsg);
    });
    connect(bus, &EventBus::sigImageSnapped, this, [this](const QString& deviceId, const QByteArray& jpegData, const QRect& location, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit imageSnapped(deviceId, jpegData, location);
    });
    connect(bus, &EventBus::sigAckReceived, this, [this](const QString& deviceId, quint8 statusCode, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit ackReceived(deviceId, statusCode);
    });
    connect(bus, &EventBus::sigDeviceReconnecting, this, [this](const QString& deviceId, int attempt, int maxRetries, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit deviceReconnecting(deviceId, attempt, maxRetries);
    });
    connect(bus, &EventBus::sigDeviceReconnectFailed, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId) emit deviceReconnectFailed(deviceId);
    });
}

bool PresenterDeviceService::acceptsEvent(const QString& deviceId, quint64 generation) const
{
    return generation == 0 || m_session->accepts(deviceId, generation);
}
