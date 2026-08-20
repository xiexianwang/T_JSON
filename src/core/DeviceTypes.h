#ifndef DEVICETYPES_H
#define DEVICETYPES_H

#include <QJsonObject>
#include <QString>
#include "DeviceState.h"

using DeviceId = QString;

struct DeviceEvent
{
    DeviceId deviceId;
    quint64 sessionGeneration = 0;
};

struct DeviceSnapshot
{
    DeviceId deviceId;
    quint64 sessionGeneration = 0;
    DeviceState state;
    bool valid = false;
};

struct DeviceJsonEvent : DeviceEvent
{
    QJsonObject payload;
};

#endif // DEVICETYPES_H
