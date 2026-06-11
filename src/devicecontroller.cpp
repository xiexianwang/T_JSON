// ============================================================
// 文件: devicecontroller.cpp
// 描述: DeviceController 类的实现。将上层业务逻辑（PTZ 控制、
//       镜头控制、算法/显示模式、预置位等）转化为具体的网络
//       指令，通过 TJsonClient 发送至设备。
// ============================================================

#include "devicecontroller.h"
#include <QDebug>

// 构造函数：保存 TJsonClient 和 ConfigManager 的指针
// 注意：两者均为非拥有指针，由外部管理其生命周期
DeviceController::DeviceController(TJsonClient* client, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cfg(cfg)
{
}

// 设置设备工作模式
// 通过 JSON 指令 "SetWorkMode" 发送模式编号
void DeviceController::setWorkMode(int mode)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SetWorkMode";
    cmd["SetWorkMode"] = mode;
    m_client->sendJsonCmd(cmd, FrameType::Control);
}

// 查询当前图像参数（无载荷，纯帧头指令）
void DeviceController::queryImageParams()
{
    m_client->sendBinaryCmd(FrameType::QueryImageParams);
}

// 设置 AI 算法模型
void DeviceController::setAlgoModel(int model)
{
    QJsonObject cmd;
    cmd["ControlType"] = "ModelSetting";
    cmd["Model"] = model;
    m_client->sendJsonCmd(cmd, FrameType::SetAlgoModel);
}

// 设置显示模式（如画中画、分屏等显示布局）
void DeviceController::setDisplayMode(int mode)
{
    QJsonObject cmd;
    cmd["ControlType"] = "PipShowSetting";
    cmd["PipShow"] = mode;
    m_client->sendJsonCmd(cmd, FrameType::SetDisplayMode);
}

// 云台方向运动
// 根据方向枚举值判断是否需要 Pan/Tilt 速度，组 Pelco-D 包后通过串口透传发送
void DeviceController::ptzMove(PtzDir dir)
{
    PtzConfig& ptz = m_cfg->ptz();
    quint8 cmd2 = static_cast<quint8>(dir);
    // 判断方向是否包含水平分量 (bit1-2) 和垂直分量 (bit3-4)
    bool hasPan = (cmd2 & 0x06) != 0;          // 有 Pan 分量
    bool hasTilt = (cmd2 & 0x18) != 0;         // 有 Tilt 分量
    quint8 panSpeed = hasPan ? ptz.panSpeed : 0x00;     // 无水平运动时速度置 0
    quint8 tiltSpeed = hasTilt ? ptz.tiltSpeed : 0x00;  // 无垂直运动时速度置 0

    QByteArray pkt = ProtocolBuilder::buildPelcoD(ptz.address, 0x00, cmd2, panSpeed, tiltSpeed);
    sendTransparentData("PELCO_D", pkt);
}

// 云台停止运动：发送 Cmd2=0x00 的停止指令
void DeviceController::ptzStop()
{
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x00, 0x00, 0x00);
    sendTransparentData("PELCO_D", pkt);
}

// 镜头变倍放大
// target=0: 可见光使用 VISCA 协议; target=1: 红外使用 Pelco-D 协议
void DeviceController::lensZoomIn(int target)
{
    m_lastLensTarget = target;      // 记录目标用于停止操作
    m_lastLensIsZoom = true;        // 标记为变倍操作
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.zoomSpeed;
    if (target == 0) {
        // 可见光：VISCA Zoom Tele
        QByteArray pkt = ProtocolBuilder::buildViscaZoom(l.visAddress, true, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变倍放大 Cmd2=0x20
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x20, 0x00, speed);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 镜头变倍缩小
void DeviceController::lensZoomOut(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = true;
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.zoomSpeed;
    if (target == 0) {
        // 可见光：VISCA Zoom Wide
        QByteArray pkt = ProtocolBuilder::buildViscaZoom(l.visAddress, false, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变倍缩小 Cmd2=0x40
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x40, 0x00, speed);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 镜头变焦拉近
void DeviceController::lensFocusIn(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = false;       // 标记为变焦操作
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.focusSpeed;
    if (target == 0) {
        // 可见光：VISCA Focus Far
        QByteArray pkt = ProtocolBuilder::buildViscaFocus(l.visAddress, true, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变焦拉近 Cmd1=0x01, Cmd2=0x00
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x01, 0x00, 0x00, 0x00);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 镜头变焦拉远
void DeviceController::lensFocusOut(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = false;
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.focusSpeed;
    if (target == 0) {
        // 可见光：VISCA Focus Near
        QByteArray pkt = ProtocolBuilder::buildViscaFocus(l.visAddress, false, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变焦拉远 Cmd2=0x80
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x80, 0x00, 0x00);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 停止镜头所有运动
// 根据上次操作的目标和类型选择对应的停止指令
void DeviceController::lensStop()
{
    LensConfig& l = m_cfg->lens();
    if (m_lastLensTarget == 0) {
        // 可见光：VISCA 停止（区分变倍停止和变焦停止）
        if (m_lastLensIsZoom)
            sendTransparentData("VISCA", ProtocolBuilder::buildViscaStop(l.visAddress, true));
        else
            sendTransparentData("VISCA", ProtocolBuilder::buildViscaStop(l.visAddress, false));
    } else {
        // 红外：Pelco-D 停止（区分变倍停止和变焦停止）
        if (m_lastLensIsZoom)
            sendTransparentData("VISCAIR", ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x60, 0x00, 0x00));
        else
            sendTransparentData("VISCAIR", ProtocolBuilder::buildPelcoD(l.irAddress, 0x01, 0x80, 0x00, 0x00));
    }
}

// ================= 预置位控制 =================

// 设置预置位：Pelco-D 命令 Set Preset (Cmd2=0x03)
void DeviceController::setPreset(int preset)
{
    if (preset < 0 || preset > 255) {
        qWarning() << "Preset out of range:" << preset;
        return;
    }
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x03, static_cast<quint8>(preset), 0x00);
    sendTransparentData("PELCO_D", pkt);
}

// 调用预置位：Pelco-D 命令 Recall Preset (Cmd2=0x07)
void DeviceController::callPreset(int preset)
{
    if (preset < 0 || preset > 255) {
        qWarning() << "Preset out of range:" << preset;
        return;
    }
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x07, static_cast<quint8>(preset), 0x00);
    sendTransparentData("PELCO_D", pkt);
}

// 删除预置位：Pelco-D 命令 Clear Preset (Cmd2=0x05)
void DeviceController::delPreset(int preset)
{
    if (preset < 0 || preset > 255) {
        qWarning() << "Preset out of range:" << preset;
        return;
    }
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x05, static_cast<quint8>(preset), 0x00);
    sendTransparentData("PELCO_D", pkt);
}

// ================= 框选跟踪 =================

// 设置目标跟踪框
// 通过 JSON 指令设置跟踪区域的中心坐标和宽高，工作模式自动切换为跟踪模式
void DeviceController::setBoxTrack(int centerX, int centerY, int width, int height)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SetWorkMode";
    cmd["SetWorkMode"] = 4;                 // 工作模式 4 对应框选跟踪

    QJsonObject center;
    center["X"] = centerX;                  // 跟踪框中心 X 坐标
    center["Y"] = centerY;                  // 跟踪框中心 Y 坐标

    QJsonObject p2;
    p2["Center"] = center;                  // 中心点
    p2["DistanceX"] = width;                // 跟踪框水平宽度
    p2["DistanceY"] = height;               // 跟踪框垂直高度

    cmd["P2Track"] = p2;
    m_client->sendJsonCmd(cmd, FrameType::Control);
}

// ================= 附加功能开关 =================

// 数字变焦开关
void DeviceController::setDigitalZoom(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "DigitalZoom";
    cmd["DigitalZoom"] = enable ? 1 : 0;
    m_client->sendJsonCmd(cmd, FrameType::SetDigitalZoom);
}

// 自动变倍开关
void DeviceController::setAutoZoom(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "AutoZoom";
    cmd["AutoZoom"] = enable ? 1 : 0;
    m_client->sendJsonCmd(cmd, FrameType::Control);
}

// 抓拍上传开关
void DeviceController::setCaptureUpload(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "CaptureState";
    cmd["CaptureState"] = enable ? 1 : 0;
    m_client->sendJsonCmd(cmd, FrameType::SetCaptureState);
}

// 位置归零（重置 PTZ 到初始位置）
void DeviceController::posReset(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "PosReset";
    cmd["PosReset"] = enable ? 1 : 0;
    m_client->sendJsonCmd(cmd, FrameType::SetPosReset);
}

// 设置设备 GPS 经纬度位置
void DeviceController::setLocation(const QString& lat, const QString& lon)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SetLatlng";
    cmd["Latitude"] = lat;                  // 纬度字符串
    cmd["Longitude"] = lon;                 // 经度字符串
    m_client->sendJsonCmd(cmd, FrameType::SetLocation);
}

// 串口透传通用网关
// 先发射 commandSent 信号供上层日志/监控，再委托给 TJsonClient 发送
void DeviceController::sendTransparentData(const QString& serialType, const QByteArray& data)
{
    emit commandSent(serialType, data);                     // 通知上层指令已发送
    m_client->sendSerialCmd(serialType, data);              // 通过 TCP 透传
}