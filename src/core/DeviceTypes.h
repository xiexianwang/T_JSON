#ifndef DEVICETYPES_H
#define DEVICETYPES_H

#include <QJsonObject>
#include <QString>
#include <memory>
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
    bool exists = false;
    bool hasState = false;
    bool hasAi = false;
    std::shared_ptr<DeviceState> statePtr;
    QJsonObject aiInfo;
};

struct DeviceJsonEvent : DeviceEvent
{
    QJsonObject payload;
};

#endif // DEVICETYPES_H
