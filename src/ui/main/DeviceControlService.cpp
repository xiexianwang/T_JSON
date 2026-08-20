#include "DeviceControlService.h"
#include "service/DeviceManager.h"
#include "service/DeviceContext.h"
#include "infrastructure/configmanager.h"

DeviceControlService::DeviceControlService(ConfigManager* cfg, QObject* parent)
    : QObject(parent), m_cfg(cfg)
{
}

// ================= PTZ 定位 =================

void DeviceControlService::ptzMoveTo(const QString& deviceId, double pan, double tilt)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->ptzMoveTo(pan, tilt);
}

void DeviceControlService::ptzSetZero(const QString& deviceId)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->ptzSetZero();
}

void DeviceControlService::setPtzOffsets(const QString& deviceId, double panOffset, double tiltOffset)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId)) {
        ctx->setPtzOffsets(panOffset, tiltOffset);
        ctx->flushZeroPosition();
    }
}

void DeviceControlService::flushZeroPosition(const QString& deviceId)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->flushZeroPosition();
}

// ================= 位置设置 =================

void DeviceControlService::setLocation(const QString& deviceId, const QString& lat, const QString& lon)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setLocation(lat, lon);
}

// ================= 图像参数 / 工作模式 / 算法 / 显示 =================

void DeviceControlService::queryImageParams(const QString& deviceId)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->queryImageParams();
}

void DeviceControlService::setWorkMode(const QString& deviceId, int mode)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setWorkMode(mode);
}

void DeviceControlService::setAlgoModel(const QString& deviceId, int model)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setAlgoModel(model);
}

void DeviceControlService::setDisplayMode(const QString& deviceId, int mode)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setDisplayMode(mode);
}

// ================= 附加功能开关 =================

void DeviceControlService::setDigitalZoom(const QString& deviceId, bool enable)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setDigitalZoom(enable);
}

void DeviceControlService::setAutoZoom(const QString& deviceId, bool enable)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setAutoZoom(enable);
}

void DeviceControlService::setCaptureUpload(const QString& deviceId, bool enable)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setCaptureUpload(enable);
}

void DeviceControlService::setPosReset(const QString& deviceId, bool enable)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->posReset(enable);
}

// ================= 框选/点选跟踪 =================

void DeviceControlService::setPointTrack(const QString& deviceId, int centerX, int centerY)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setPointTrack(centerX, centerY);
}

void DeviceControlService::setBoxTrack(const QString& deviceId, int centerX, int centerY, int width, int height)
{
    if (DeviceContext* ctx = DeviceManager::instance().getDevice(deviceId))
        ctx->setBoxTrack(centerX, centerY, width, height);
}
