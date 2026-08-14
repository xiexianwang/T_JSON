#include "EventBus.h"
#include <QMutex>
#include <QMutexLocker>



EventBus* EventBus::instance()
{
    static EventBus* instance = new EventBus(); return instance;
}

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
}

void EventBus::postDeviceConnected(const QString& deviceId)
{
    emit sigDeviceConnected(deviceId);
}

void EventBus::postDeviceDisconnected(const QString& deviceId)
{
    emit sigDeviceDisconnected(deviceId);
}

void EventBus::postDeviceError(const QString& deviceId, const QString& errorMsg)
{
    emit sigDeviceError(deviceId, errorMsg);
}

void EventBus::postJsonReceived(const QString& deviceId, const QJsonObject& doc)
{
    emit sigJsonReceived(deviceId, doc);
}

void EventBus::postImageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location)
{
    emit sigImageSnapped(deviceId, jpegData, location);
}

void EventBus::postPtzUpdated(const QString& deviceId, double pan, double tilt, double zoom)
{
    emit sigPtzUpdated(deviceId, pan, tilt, zoom);
}

void EventBus::postDeviceAiTimeout(const QString& deviceId)
{
    emit sigDeviceAiTimeout(deviceId);
}

void EventBus::postDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state)
{
    emit sigDeviceStateUpdated(deviceId, state);
}

void EventBus::postDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc)
{
    emit sigDeviceAiInfoUpdated(deviceId, aiDoc);
}

void EventBus::postDeviceFrameReady(const QString& deviceId, const QImage& frame)
{
    emit sigDeviceFrameReady(deviceId, frame);
}


void EventBus::postRtspOpened(const QString& deviceId)
{
    emit sigRtspOpened(deviceId);
}

void EventBus::postRtspError(const QString& deviceId, const QString& errorMsg)
{
    emit sigRtspError(deviceId, errorMsg);
}

void EventBus::postAckReceived(const QString& deviceId, quint8 statusCode) { emit sigAckReceived(deviceId, statusCode); }
void EventBus::postDeviceReconnecting(const QString& deviceId, int attempt, int maxRetries) { emit sigDeviceReconnecting(deviceId, attempt, maxRetries); }
void EventBus::postDeviceReconnectFailed(const QString& deviceId) { emit sigDeviceReconnectFailed(deviceId); }
