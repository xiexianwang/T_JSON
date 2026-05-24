#include "devicecontroller.h"

DeviceController::DeviceController(TJsonClient* client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

void DeviceController::setWorkMode(int mode)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SetWorkMode";
    cmd["SetWorkMode"] = mode;
    m_client->sendJsonCmd(cmd, FrameType::Control);
}

void DeviceController::queryImageParams()
{
    m_client->sendBinaryCmd(FrameType::QueryImageParams);
}

void DeviceController::sendTransparentData(const QString& serialType, const QByteArray& data)
{
    // 调用底层的 JSON 透传组包接口
    m_client->sendSerialCmd(serialType, data);
}