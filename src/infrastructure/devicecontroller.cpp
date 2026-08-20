// ============================================================
// 文件: devicecontroller.cpp
// 描述: DeviceController 类的实现。负责协议选择与业务编排：
//       JSON 设备指令、PTZ/镜头/预置位组包透传、电机指令委托。
// ============================================================

#include "devicecontroller.h"
#include "devicecommandservice.h"
#include <QDebug>
#include <QTimer>

// 构造函数：保存 TJsonClient 和 ConfigManager 的指针，创建电机指令服务
// 注意：TJsonClient/ConfigManager 均为非拥有指针，由外部管理其生命周期
DeviceController::DeviceController(TJsonClient* client, ConfigManager* cfg, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_cfg(cfg)
{
    m_motorService = new DeviceCommandService(cfg, this);
    // 注入 Pelco-D 透传回调：电机走 PELCO_D 通道时经 TJsonClient 串口透传
    m_motorService->setPelcoDSender([this](const QByteArray& pkt) {
        sendTransparentData("PELCO_D", pkt);
    });

    // 转发电机指令服务的信号，保持本类对外信号接口不变
    connect(m_motorService, &DeviceCommandService::commandSent,
            this, &DeviceController::commandSent);
    connect(m_motorService, &DeviceCommandService::motorModeResult,
            this, &DeviceController::motorModeResult);
    connect(m_motorService, &DeviceCommandService::motorSilentResult,
            this, &DeviceController::motorSilentResult);
    connect(m_motorService, &DeviceCommandService::motorSerialError,
            this, &DeviceController::motorSerialError);
    connect(m_motorService, &DeviceCommandService::motorTcpError,
            this, &DeviceController::motorTcpError);
}

// 设置设备工作模式
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
    cmd["PipShow"] = (mode >= 0 && mode < kPipShowCount) ? kPipShowValues[mode] : mode;
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

    QByteArray pkt = PelcoDProtocol::buildMove(ptz.address, dir, panSpeed, tiltSpeed);
    sendTransparentData("PELCO_D", pkt);
}

// 云台停止运动：发送 Cmd2=0x00 的停止指令
void DeviceController::ptzStop()
{
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = PelcoDProtocol::buildStop(addr);
    sendTransparentData("PELCO_D", pkt);
}

// 云台转动到绝对角度
void DeviceController::ptzMoveTo(double pan, double tilt)
{
    quint8 addr = m_cfg->ptz().address;

    // 如果开启了模拟串口服务器，则应用软件偏置
    if (m_cfg->softwarePtzCalibrationEnabled()) {
        pan += m_cfg->ptzPanOffset();
        tilt += m_cfg->ptzTiltOffset();
    }
    
    while (pan >= 360.0) pan -= 360.0;
    while (pan < 0) pan += 360.0;

    while (tilt > 180.0) tilt -= 360.0;
    while (tilt <= -180.0) tilt += 360.0;

    int panVal = static_cast<int>(pan * 100);
    QByteArray panPkt = PelcoDProtocol::buildPanTo(addr, panVal);
    sendTransparentData("PELCO_D", panPkt);

    int tiltVal = tilt >= 0 ? static_cast<int>(tilt * 100 + 0.5) : static_cast<int>(36000 + tilt * 100 + 0.5);
    QByteArray tiltPkt = PelcoDProtocol::buildTiltTo(addr, tiltVal);
    QTimer::singleShot(50, this, [this, tiltPkt]() {
        sendTransparentData("PELCO_D", tiltPkt);
    });
}

// 云台水平零点标定 (Pelco-D: 0x49)
void DeviceController::ptzSetZero()
{
    quint8 addr = m_cfg->ptz().address;
    QByteArray pkt = PelcoDProtocol::buildSetZero(addr);
    sendTransparentData("PELCO_D", pkt);
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
        QByteArray pkt = ViscaProtocol::buildZoom(l.visAddress, false, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变倍缩小 Cmd2=0x40
        QByteArray pkt = PelcoDProtocol::buildZoomOut(l.irAddress, speed);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 镜头变倍放大
void DeviceController::lensZoomIn(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = true;
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.zoomSpeed;
    if (target == 0) {
        QByteArray pkt = ViscaProtocol::buildZoom(l.visAddress, true, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        QByteArray pkt = PelcoDProtocol::buildZoomIn(l.irAddress, speed);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 镜头变焦拉近
void DeviceController::lensFocusIn(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = false;       // 标记为变焦操作
    LensConfig& l = m_cfg->lens();
    if (target == 0) {
        // 可见光：VISCA Focus Far
        QByteArray pkt = ViscaProtocol::buildFocus(l.visAddress, true);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变焦拉近 Cmd1=0x01, Cmd2=0x00
        QByteArray pkt = PelcoDProtocol::buildFocusIn(l.irAddress);
        sendTransparentData("VISCAIR", pkt);
    }
}

// 镜头变焦拉远
void DeviceController::lensFocusOut(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = false;
    LensConfig& l = m_cfg->lens();
    if (target == 0) {
        // 可见光：VISCA Focus Near
        QByteArray pkt = ViscaProtocol::buildFocus(l.visAddress, false);
        sendTransparentData("VISCA", pkt);
    } else {
        // 红外：Pelco-D 变焦拉远 Cmd2=0x80
        QByteArray pkt = PelcoDProtocol::buildFocusOut(l.irAddress);
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
            sendTransparentData("VISCA", ViscaProtocol::buildStop(l.visAddress, true));
        else
            sendTransparentData("VISCA", ViscaProtocol::buildStop(l.visAddress, false));
    } else {
        // 红外：Pelco-D 停止（区分变倍停止和变焦停止）
        if (m_lastLensIsZoom)
            sendTransparentData("VISCAIR", PelcoDProtocol::buildLensStop(l.irAddress, true));
        else
            sendTransparentData("VISCAIR", PelcoDProtocol::buildLensStop(l.irAddress, false));
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
    QByteArray pkt = PelcoDProtocol::buildSetPreset(addr, static_cast<quint8>(preset));
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
    QByteArray pkt = PelcoDProtocol::buildCallPreset(addr, static_cast<quint8>(preset));
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
    QByteArray pkt = PelcoDProtocol::buildClearPreset(addr, static_cast<quint8>(preset));
    sendTransparentData("PELCO_D", pkt);
}

// ================= 框选跟踪 =================

// 设置目标跟踪框
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

// 点选跟踪
void DeviceController::setPointTrack(int centerX, int centerY)
{
    QJsonObject cmd;
    cmd["ControlType"] = "SetWorkMode";
    cmd["SetWorkMode"] = 3;

    QJsonObject center;
    center["X"] = centerX;
    center["Y"] = centerY;

    QJsonObject p2;
    p2["Center"] = center;
    p2["Distance"] = 30;

    cmd["P2Track"] = p2;
    m_client->sendJsonCmd(cmd, FrameType::Control);
}

// ================= 附加功能开关 =================

// 数字变焦开关
void DeviceController::setDigitalZoom(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "DigitalZoomSetting";
    cmd["DigitalZoom"] = enable ? 1 : 0;
    m_client->sendJsonCmd(cmd, FrameType::SetDigitalZoom);
}

// 自动变焦开关
void DeviceController::setAutoZoom(bool enable)
{
    setWorkMode(enable ? 5 : 6);
}

// 抓拍上传开关
void DeviceController::setCaptureUpload(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "ImageUpload";
    cmd["Upload"] = enable ? 1 : 0;
    m_client->sendJsonCmd(cmd, FrameType::SetCaptureState);
}

// 雨刷开关
void DeviceController::setWiper(bool enable)
{
    if (enable) motorStart();
    else motorStop();
}

// 位置归零（重置 PTZ 到初始位置）
void DeviceController::posReset(bool enable)
{
    QJsonObject cmd;
    cmd["ControlType"] = "ResetPosition";
    cmd["ResetPosition"] = enable ? 1 : 0;
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

// ================= 电机通道管理（委托 DeviceCommandService） =================

bool DeviceController::openMotorSerial(const QString& portName)
{
    return m_motorService->openMotorSerial(portName);
}

void DeviceController::closeMotorSerial()
{
    m_motorService->closeMotorSerial();
}

bool DeviceController::isMotorSerialOpen() const
{
    return m_motorService->isMotorSerialOpen();
}

void DeviceController::openMotorTcp()
{
    m_motorService->openMotorTcp();
}

void DeviceController::closeMotorTcp()
{
    m_motorService->closeMotorTcp();
}

bool DeviceController::isMotorTcpOpen() const
{
    return m_motorService->isMotorTcpOpen();
}

// ================= 雨刷电机控制（委托 DeviceCommandService） =================

void DeviceController::motorStart() { m_motorService->motorStart(); }
void DeviceController::motorStop() { m_motorService->motorStop(); }
void DeviceController::motorWiperStop() { m_motorService->motorWiperStop(); }
void DeviceController::motorJogLeft() { m_motorService->motorJogLeft(); }
void DeviceController::motorJogRight() { m_motorService->motorJogRight(); }
void DeviceController::motorZeroCalib() { m_motorService->motorZeroCalib(); }
void DeviceController::motorReturnZero() { m_motorService->motorReturnZero(); }
void DeviceController::motorCheckMode() { m_motorService->motorCheckMode(); }
void DeviceController::motorToggleMode() { m_motorService->motorToggleMode(); }
void DeviceController::motorToggleSilentMode() { m_motorService->motorToggleSilentMode(); }
void DeviceController::motorSetCurrent(int ma) { m_motorService->motorSetCurrent(ma); }
