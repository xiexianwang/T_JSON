#include "devicecontroller.h"

DeviceController::DeviceController(TJsonClient* client, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cfg(cfg)
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

void DeviceController::setAlgoModel(int model)
{
    QJsonObject cmd;
    cmd["ControlType"] = "ModelSetting";
    cmd["Model"] = model;
    m_client->sendJsonCmd(cmd, FrameType::SetAlgoModel);
}

void DeviceController::setDisplayMode(int mode)
{
    QJsonObject cmd;
    cmd["ControlType"] = "PipShowSetting";
    cmd["PipShow"] = mode;
    m_client->sendJsonCmd(cmd, FrameType::SetDisplayMode);
}

void DeviceController::ptzMove(PtzDir dir)
{
    PtzConfig& ptz = m_cfg->ptz();
    quint8 cmd2 = static_cast<quint8>(dir);
    bool hasPan = (cmd2 & 0x06) != 0;
    bool hasTilt = (cmd2 & 0x18) != 0;
    quint8 panSpeed = hasPan ? ptz.panSpeed : 0x00;
    quint8 tiltSpeed = hasTilt ? ptz.tiltSpeed : 0x00;

    QByteArray pkt = ProtocolBuilder::buildPelcoD(ptz.address, 0x00, cmd2, panSpeed, tiltSpeed);
    sendTransparentData("PELCO_D", pkt);
}

void DeviceController::ptzStop()
{
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x00, 0x00, 0x00);
    sendTransparentData("PELCO_D", pkt);
}

void DeviceController::lensZoomIn(int target)
{
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.zoomSpeed;
    if (target == 0) {
        QByteArray pkt = ProtocolBuilder::buildViscaZoom(l.visAddress, true, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x20, 0x00, speed);
        sendTransparentData("PELCO_D", pkt);
    }
}

void DeviceController::lensZoomOut(int target)
{
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.zoomSpeed;
    if (target == 0) {
        QByteArray pkt = ProtocolBuilder::buildViscaZoom(l.visAddress, false, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x40, 0x00, speed);
        sendTransparentData("PELCO_D", pkt);
    }
}

void DeviceController::lensFocusIn(int target)
{
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.focusSpeed;
    if (target == 0) {
        QByteArray pkt = ProtocolBuilder::buildViscaFocus(l.visAddress, true, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x80, 0x00, 0x00, speed);
        sendTransparentData("PELCO_D", pkt);
    }
}

void DeviceController::lensFocusOut(int target)
{
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.focusSpeed;
    if (target == 0) {
        QByteArray pkt = ProtocolBuilder::buildViscaFocus(l.visAddress, false, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x40, 0x00, 0x00, speed);
        sendTransparentData("PELCO_D", pkt);
    }
}

void DeviceController::lensStop()
{
    LensConfig& l = m_cfg->lens();
    QByteArray pkt1 = ProtocolBuilder::buildViscaStop(l.visAddress, true);
    sendTransparentData("VISCA", pkt1);
    QByteArray pkt2 = ProtocolBuilder::buildViscaStop(l.visAddress, false);
    sendTransparentData("VISCA", pkt2);
    QByteArray pkt3 = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x00, 0x00, 0x00);
    sendTransparentData("PELCO_D", pkt3);
}

void DeviceController::setLocation(const QString& lat, const QString& lon)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SetLatlng";
    cmd["Latitude"] = lat;
    cmd["Longitude"] = lon;
    m_client->sendJsonCmd(cmd, FrameType::SetLocation);
}

void DeviceController::sendTransparentData(const QString& serialType, const QByteArray& data)
{
    // 调用底层的 JSON 透传组包接口
    m_client->sendSerialCmd(serialType, data);
}