#include "PresenterMotorService.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "ui/main/IMainView.h"
#include "infrastructure/configmanager.h"
#include <QTimer>

PresenterMotorService::PresenterMotorService(IMainView* view, ConfigManager* cfg, QObject *parent)
    : QObject(parent), m_view(view), m_cfg(cfg)
{
}

DeviceContext* PresenterMotorService::getCtx(const QString& deviceId) const
{
    return DeviceManager::instance().getDevice(deviceId);
}

void PresenterMotorService::ptzMove(const QString& deviceId, int direction)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->ptzMove(static_cast<PtzDir>(direction));
}

void PresenterMotorService::ptzStop(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->ptzStop();
}

void PresenterMotorService::lensMove(const QString& deviceId, int op)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    int target = (m_view->displayModeIndex() == 1 || m_view->displayModeIndex() == 4) ? 1 : 0;
    switch (op) {
        case 0: ctx->lensZoomIn(target); break;
        case 1: ctx->lensZoomOut(target); break;
        case 2: ctx->lensFocusIn(target); break;
        case 3: ctx->lensFocusOut(target); break;
    }
}

void PresenterMotorService::lensStop(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->lensStop();
}

void PresenterMotorService::initMotorChannel(const QString& deviceId)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    const DeviceConfig& cfg = ctx->deviceConfig();
    if (cfg.motorSerialEnabled && cfg.motorProtocol == "MODBUS-RTU" && cfg.motorCommandChannel == "串口") {
        ctx->openMotorSerial(cfg.motorComPort);
    } else if (cfg.motorIpEnabled && cfg.motorProtocol == "STM32-TCP-V4.0") {
        ctx->openMotorTcp();
    }
}

void PresenterMotorService::applyMotorChannel(const QString& deviceId)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    const DeviceConfig& cfg = ctx->deviceConfig();
    if (cfg.motorSerialEnabled && cfg.motorProtocol == "MODBUS-RTU" && cfg.motorCommandChannel == "串口") {
        ctx->openMotorSerial(cfg.motorComPort);
        ctx->closeMotorTcp();
    } else if (cfg.motorIpEnabled && cfg.motorProtocol == "STM32-TCP-V4.0") {
        ctx->openMotorTcp();
        ctx->closeMotorSerial();
    } else {
        ctx->closeMotorSerial();
        ctx->closeMotorTcp();
    }
}

void PresenterMotorService::initPtzForwarder(const QString& deviceId)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    ctx->startPtzForwarder();
}

void PresenterMotorService::onWiperStart(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorStart();
}

void PresenterMotorService::onWiperStop(const QString& deviceId)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    ctx->motorWiperStop();
}

void PresenterMotorService::onWiperJogLeft(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorJogLeft();
}

void PresenterMotorService::onWiperJogRight(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorJogRight();
}

void PresenterMotorService::onWiperJogStop(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorStop();
}

void PresenterMotorService::onWiperZeroCalib(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorZeroCalib();
}

void PresenterMotorService::onWiperMode(const QString& deviceId)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    ctx->motorToggleMode();
    QTimer::singleShot(500, this, [this, deviceId]() {
        if (DeviceContext* c = getCtx(deviceId)) c->motorCheckMode();
    });
}

void PresenterMotorService::onWiperSilent(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorToggleSilentMode();
}

void PresenterMotorService::onWiperCurrentSet(const QString& deviceId, int run, int hold, int delay)
{
    if (DeviceContext* ctx = getCtx(deviceId)) {
        // 按协议约束电流参数：
        //   STM32-TCP-V4.0：run/hold ∈ 1-31，delay ∈ 0-15
        //   MODBUS-RTU/Pelco-D：仅运行电流(mA) ≤ 2000，保持/延迟不适用
        const QString proto = ctx->deviceConfig().motorProtocol;
        if (proto == "STM32-TCP-V4.0") {
            run = qBound(1, run, 31);
            hold = qBound(1, hold, 31);
            delay = qBound(0, delay, 15);
        } else {
            run = qBound(0, run, 2000);
        }
        ctx->motorSetCurrent(run, hold, delay);
    }
}

void PresenterMotorService::checkMotorMode(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorCheckMode();
}

void PresenterMotorService::readMotorCurrent(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorReadCurrent();
}

void PresenterMotorService::callPreset(const QString& deviceId, int preset)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->callPreset(preset);
}

void PresenterMotorService::setPreset(const QString& deviceId, int preset)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->setPreset(preset);
}

void PresenterMotorService::delPreset(const QString& deviceId, int preset)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->delPreset(preset);
}
