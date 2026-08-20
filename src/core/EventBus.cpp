#include "EventBus.h"
#include <QMutex>
#include <QMutexLocker>



EventBus& EventBus::instance()
{
    static EventBus inst;
    return inst;
}

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
}

void EventBus::postDeviceConnected(const QString& deviceId, quint64 generation)
{
    emit sigDeviceConnected(deviceId, generation);
}

void EventBus::postDeviceDisconnected(const QString& deviceId, quint64 generation)
{
    emit sigDeviceDisconnected(deviceId, generation);
}

void EventBus::postDeviceError(const QString& deviceId, const QString& errorMsg, quint64 generation)
{
    emit sigDeviceError(deviceId, errorMsg, generation);
}

void EventBus::postJsonReceived(const QString& deviceId, const QJsonObject& doc, quint64 generation)
{
    emit sigJsonReceived(deviceId, doc, generation);
}

void EventBus::postImageSnapped(const QString& deviceId, const QByteArray& jpegData, const QRect& location, quint64 generation)
{
    emit sigImageSnapped(deviceId, jpegData, location, generation);
}

void EventBus::postPtzUpdated(const QString& deviceId, double pan, double tilt, double zoom, quint64 generation)
{
    emit sigPtzUpdated(deviceId, pan, tilt, zoom, generation);
}

void EventBus::postDeviceAiTimeout(const QString& deviceId, quint64 generation)
{
    emit sigDeviceAiTimeout(deviceId, generation);
}

void EventBus::postDeviceStateUpdated(const QString& deviceId, std::shared_ptr<DeviceState> state, quint64 generation)
{
    emit sigDeviceStateUpdated(deviceId, state, generation);
}

void EventBus::postDeviceAiInfoUpdated(const QString& deviceId, const QJsonObject& aiDoc, quint64 generation)
{
    emit sigDeviceAiInfoUpdated(deviceId, aiDoc, generation);
}

void EventBus::postDeviceFrameReady(const QString& deviceId, const QImage& frame, quint64 generation)
{
    emit sigDeviceFrameReady(deviceId, frame, generation);
}


void EventBus::postRtspOpened(const QString& deviceId, quint64 generation)
{
    emit sigRtspOpened(deviceId, generation);
}

void EventBus::postRtspError(const QString& deviceId, const QString& errorMsg, quint64 generation)
{
    emit sigRtspError(deviceId, errorMsg, generation);
}

void EventBus::postAckReceived(const QString& deviceId, quint8 statusCode, quint64 generation) { emit sigAckReceived(deviceId, statusCode, generation); }
void EventBus::postDeviceReconnecting(const QString& deviceId, int attempt, int maxRetries, quint64 generation) { emit sigDeviceReconnecting(deviceId, attempt, maxRetries, generation); }
void EventBus::postDeviceReconnectFailed(const QString& deviceId, quint64 generation) { emit sigDeviceReconnectFailed(deviceId, generation); }
