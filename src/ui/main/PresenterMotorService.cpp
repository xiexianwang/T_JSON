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
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        ctx->openMotorSerial(m_cfg->motorComPort());
    } else if (m_cfg->motorIpEnabled() && m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        ctx->openMotorTcp();
    }
}

void PresenterMotorService::applyMotorChannel(const QString& deviceId)
{
    DeviceContext* ctx = getCtx(deviceId);
    if (!ctx) return;
    if (m_cfg->motorSerialEnabled() && m_cfg->motorProtocol() == "MODBUS-RTU" && m_cfg->motorCommandChannel() == "串口") {
        ctx->openMotorSerial(m_cfg->motorComPort());
        ctx->closeMotorTcp();
    } else if (m_cfg->motorIpEnabled() && m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
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
    ctx->startPtzForwarder(m_cfg->serialIp(), m_cfg->serialPort(), m_cfg->mockServerPort());
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

void PresenterMotorService::onWiperCurrentSet(const QString& deviceId, int ma)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorSetCurrent(ma);
}

void PresenterMotorService::checkMotorMode(const QString& deviceId)
{
    if (DeviceContext* ctx = getCtx(deviceId)) ctx->motorCheckMode();
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
