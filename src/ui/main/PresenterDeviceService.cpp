#include "PresenterDeviceService.h"
#include "PresenterMotorService.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "ui/main/IMainView.h"
#include "core/EventBus.h"
#include "infrastructure/configmanager.h"
#include "ui/main/DeviceSessionService.h"

PresenterDeviceService::PresenterDeviceService(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent), m_view(view), m_cfg(cfg)
    , m_session(new DeviceSessionService(cfg, this))
{
    // 不再创建 "default_device" 幽灵设备。
    // 当前设备在首次 activateDevice() 时确定，之前 currentDevice() 返回 nullptr。
}

PresenterDeviceService::~PresenterDeviceService()
{
}

DeviceContext* PresenterDeviceService::ensureContext(const QString& deviceId,
                                                     const DeviceEntry& entry)
{
    if (deviceId.isEmpty()) return nullptr;

    DeviceContext* ctx = m_session->ensureDevice(deviceId);
    if (!ctx) ctx = DeviceManager::instance().addDevice(deviceId);
    if (!ctx) return nullptr;

    if (entry.id == deviceId) {
        ctx->setDeviceConfig(entry.config);
    }
    return ctx;
}

void PresenterDeviceService::activateDevice(const QString& deviceId, const DeviceEntry& entry)
{
    if (deviceId.isEmpty()) return;

    const bool deviceChanged = (m_currentDeviceId != deviceId);

    DeviceContext* ctx = ensureContext(deviceId, entry);
    if (!ctx) return;

    // 已在线的同一设备（如再次双击）：直接复用，避免无谓地断/连抖动
    if (!deviceChanged && ctx->isConnected() && ctx->isVideoRunning()) {
        return;
    }

    if (deviceChanged) {
        // 旧设备保留 TCP/RTSP（多设备并行在线），仅释放电机/转台：
        // 电机指令通道同一时刻只能作用于一个设备。
        DeviceContext* oldCtx = DeviceManager::instance().getDevice(m_currentDeviceId);
        disconnectDeviceSignals();
        resetForSwitch(oldCtx);

        m_session->selectDevice(deviceId);
        m_currentDeviceId = deviceId;
    }

    // 先幂等重绑电机/电流/指令日志信号，再通知切换：
    // 保证 onDeviceSwitched 立即读到的是新设备通道（同一设备重连也必须在场，
    // 否则电机状态上报与指令日志会永久失效）。
    disconnectDeviceSignals();
    connectDeviceSignals(ctx);

    // 无论切换还是同设备重连，都刷新视频流状态文本（避免残留旧状态）
    m_view->setVideoStatusText(deviceId, QString());

    if (deviceChanged) emit deviceSwitched();

    // 无可用视频槽位时提示用户（TCP 仍会连接，但不会有画面）
    if (!m_view->setVideoFrame(deviceId, QImage())) {
        emit videoSlotUnavailable(deviceId);
    }

    ctx->connectDevice(entry.ip, m_cfg->deviceTcpPort());
    if (!entry.rtspUrl.isEmpty()) {
        ctx->startVideo(entry.rtspUrl);
    }
}

// 仅切换当前设备焦点：不触碰 TCP/RTSP（避免视频闪断）。
// 前置条件：目标设备已连接（未连接时由上层走 activateDevice）。
void PresenterDeviceService::focusDevice(const QString& deviceId)
{
    if (deviceId.isEmpty()) return;
    if (deviceId == m_currentDeviceId) return;

    DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
    if (!ctx || !ctx->isConnected()) return;

    // 旧设备保留 TCP/RTSP（多设备并行在线），仅释放电机/转台并清理当前展示。
    DeviceContext* oldCtx = DeviceManager::instance().getDevice(m_currentDeviceId);
    disconnectDeviceSignals();
    resetForSwitch(oldCtx);

    m_session->selectDevice(deviceId);
    m_currentDeviceId = deviceId;

    // 重新绑定新设备的电机/状态/日志信号
    disconnectDeviceSignals();
    connectDeviceSignals(ctx);

    emit deviceSwitched();
}

void PresenterDeviceService::toggleDeviceConnect(const QString& deviceId, const DeviceEntry& entry)
{
    if (deviceId.isEmpty()) return;

    DeviceContext* ctx = ensureContext(deviceId, entry);
    if (!ctx) return;

    // 已连接：断开该设备（保留上下文、树节点与视频槽位）
    if (ctx->isConnected()) {
        disconnectDevice(deviceId);
        return;
    }

    // 未连接：统一走激活路径（内部完成信号重绑、焦点切换、TCP/RTSP 启动）
    activateDevice(deviceId, entry);
}

void PresenterDeviceService::disconnectDevice(const QString& deviceId)
{
    DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
    if (!ctx) return;

    ctx->stopVideo();
    ctx->disconnectNetwork();
    ctx->closeMotorTcp();
    ctx->closeMotorSerial();
    if (deviceId == m_currentDeviceId)
        disconnectDeviceSignals();
    // 清帧但保留槽位（设备仍存在于设备树）
    m_view->setVideoFrame(deviceId, QImage());
}

bool PresenterDeviceService::isDeviceConnected(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
    return ctx && ctx->isConnected();
}

DeviceContext* PresenterDeviceService::currentDevice() const
{
    if (m_currentDeviceId.isEmpty()) return nullptr;
    return DeviceManager::instance().getDevice(m_currentDeviceId);
}

bool PresenterDeviceService::isMotorSerialOpen(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
    return ctx && ctx->isMotorSerialOpen();
}

bool PresenterDeviceService::isMotorTcpOpen(const QString& deviceId) const
{
    DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
    return ctx && ctx->isMotorTcpOpen();
}

void PresenterDeviceService::resetForSwitch(DeviceContext* oldCtx)
{
    if (oldCtx) {
        oldCtx->closeMotorTcp();
        oldCtx->closeMotorSerial();
    }

    m_view->mapClearAllTracks();
    m_view->mapUpdateTargetMarkers(QJsonArray());
    m_view->mapClearFov();
    m_view->clearIdentifyTable();
    m_view->setIdentifyCount(QString());
    m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
    m_view->setTrackPos(QString());
    m_view->setTrackDistance(QString());
    m_view->setTrackMissDistance(QString());
}

void PresenterDeviceService::removeDevice(const QString& deviceId)
{
    if (deviceId.isEmpty()) return;

    const bool wasCurrent = (m_currentDeviceId == deviceId);

    if (wasCurrent) {
        disconnectDeviceSignals();
        m_view->mapClearAllTracks();
        m_view->mapUpdateTargetMarkers(QJsonArray());
        m_view->mapClearFov();
        m_view->clearIdentifyTable();
        m_view->setIdentifyCount(QString());
        m_view->showTrackStatus(QString::fromUtf8("状态: 未锁定"), "nolock");
        m_view->setTrackPos(QString());
        m_view->setTrackDistance(QString());
        m_view->setTrackMissDistance(QString());
    }

    m_view->clearVideoFrame(deviceId);

    if (!m_session->removeDevice(deviceId)) {
        // 上下文 shutdown 未完成（DeviceManager 拒绝删除）。
        // 明确提示用户：树节点已移除但运行态仍在，需重试或重启客户端。
        m_view->showStatusMessage(
            QString::fromUtf8("设备资源未完全释放，删除未生效，请重试"), 5000);
        return;
    }

    if (!wasCurrent) return;

    const QList<QString> remaining = DeviceManager::instance().getAllDeviceIds();
    if (remaining.isEmpty()) {
        m_currentDeviceId.clear();
        emit deviceSwitched();
        m_view->showStatusMessage(QString::fromUtf8("所有设备已断开"), 3000);
        return;
    }

    const QString nextId = remaining.first();
    DeviceContext* nextCtx = DeviceManager::instance().getDevice(nextId);
    m_session->selectDevice(nextId);
    m_currentDeviceId = nextId;
    connectDeviceSignals(nextCtx);
    m_view->setVideoFrame(nextId, QImage());
    emit deviceSwitched();
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
    m_motorCurrentConn = connect(ctx, &DeviceContext::motorCurrentResult, this,
                                 [this, deviceId](int run, int hold, int delay) {
        emit motorCurrentChanged(deviceId, run, hold, delay);
    });
    m_commandLogConn = connect(ctx, &DeviceContext::commandSent, this,
                               [this, deviceId](const QString& type, const QByteArray& data) {
        emit commandSent(deviceId, type, data);
    });
    m_rtspStatsConn = connect(ctx, &DeviceContext::rtspStatsChanged, this,
                              [this, deviceId](const RtspThread::Stats& stats) {
        emit rtspStatsChanged(deviceId, stats);
    });
}

void PresenterDeviceService::disconnectDeviceSignals()
{
    disconnect(m_motorModeConn);
    disconnect(m_motorErrorConn);
    disconnect(m_motorTcpErrorConn);
    disconnect(m_motorSilentConn);
    disconnect(m_motorCurrentConn);
    disconnect(m_commandLogConn);
    disconnect(m_rtspStatsConn);
}

// ============================================================================
// EventBus 连接
// ============================================================================
void PresenterDeviceService::setupEventBus()
{
    const auto* bus = &EventBus::instance();

    // 帧就绪：所有活跃设备的帧都路由到各自 VideoWidget（多设备并行显示）
    connect(bus, &EventBus::sigDeviceFrameReady, this,
            [this](const QString& deviceId, const QImage& frame, quint64 generation) {
        if (acceptsEvent(deviceId, generation))
            emit deviceFrameReady(deviceId, frame);
    });

    connect(bus, &EventBus::sigDeviceConnected, this, [this](const QString& deviceId, quint64 generation) {
        if (!acceptsEvent(deviceId, generation)) return;
        if (deviceId == m_currentDeviceId) {
            DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId);
            if (ctx) {
                ctx->queryImageParams();
                const DeviceConfig& dev = ctx->deviceConfig();
                ctx->setDigitalZoom(dev.digitalZoom);
                ctx->setAutoZoom(dev.autoZoom);
                ctx->setCaptureUpload(dev.captureUpload);
                ctx->posReset(dev.posReset);
            }
            emit deviceConnected(deviceId);
        }
    });

    connect(bus, &EventBus::sigDeviceDisconnected, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit deviceDisconnected(deviceId);
    });

    connect(bus, &EventBus::sigRtspOpened, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit rtspOpened(deviceId);
    });
    connect(bus, &EventBus::sigRtspError, this, [this](const QString& deviceId, const QString& msg, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit rtspError(deviceId, msg);
    });

    connect(bus, &EventBus::sigDeviceStateUpdated, this,
            [this](const QString& deviceId, std::shared_ptr<DeviceState> state, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId && state)
            emit deviceStateUpdated(deviceId, state);
    });
    connect(bus, &EventBus::sigDeviceAiInfoUpdated, this,
            [this](const QString& deviceId, const QJsonObject& aiDoc, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit deviceAiInfoUpdated(deviceId, aiDoc);
    });
    connect(bus, &EventBus::sigDeviceAiTimeout, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit deviceAiTimeout(deviceId);
    });

    connect(bus, &EventBus::sigDeviceError, this, [this](const QString& deviceId, const QString& errorMsg, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit deviceError(deviceId, errorMsg);
    });
    connect(bus, &EventBus::sigImageSnapped, this,
            [this](const QString& deviceId, const QByteArray& jpegData, const QRect& location, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit imageSnapped(deviceId, jpegData, location);
    });
    connect(bus, &EventBus::sigAckReceived, this, [this](const QString& deviceId, quint8 statusCode, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit ackReceived(deviceId, statusCode);
    });
    connect(bus, &EventBus::sigDeviceReconnecting, this,
            [this](const QString& deviceId, int attempt, int maxRetries, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit deviceReconnecting(deviceId, attempt, maxRetries);
    });
    connect(bus, &EventBus::sigDeviceReconnectFailed, this, [this](const QString& deviceId, quint64 generation) {
        if (acceptsEvent(deviceId, generation) && deviceId == m_currentDeviceId)
            emit deviceReconnectFailed(deviceId);
    });
}

bool PresenterDeviceService::acceptsEvent(const QString& deviceId, quint64 generation) const
{
    return generation == 0 || m_session->accepts(deviceId, generation);
}
