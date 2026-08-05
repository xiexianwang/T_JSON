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
