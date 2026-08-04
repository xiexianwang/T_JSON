// ============================================================
// 文件: devicecontroller.cpp
// 描述: DeviceController 类的实现。将上层业务逻辑（PTZ 控制、
//       镜头控制、算法/显示模式、预置位等）转化为具体的网络
//       指令，通过 TJsonClient 发送至设备。
// ============================================================

#include "devicecontroller.h"
#include <QDebug>
#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>

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
    QByteArray panPkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x4B, (panVal >> 8) & 0xFF, panVal & 0xFF);
    sendTransparentData("PELCO_D", panPkt);

    int tiltVal = tilt >= 0 ? static_cast<int>(tilt * 100 + 0.5) : static_cast<int>(36000 + tilt * 100 + 0.5);
    QByteArray tiltPkt = ProtocolBuilder::buildPelcoD(addr, 0x00, 0x4D, (tiltVal >> 8) & 0xFF, tiltVal & 0xFF);
    QTimer::singleShot(50, this, [this, tiltPkt]() {
        sendTransparentData("PELCO_D", tiltPkt);
    });
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

// 镜头变倍放大
void DeviceController::lensZoomIn(int target)
{
    m_lastLensTarget = target;
    m_lastLensIsZoom = true;
    LensConfig& l = m_cfg->lens();
    quint8 speed = l.zoomSpeed;
    if (target == 0) {
        QByteArray pkt = ProtocolBuilder::buildViscaZoom(l.visAddress, true, speed);
        sendTransparentData("VISCA", pkt);
    } else {
        QByteArray pkt = ProtocolBuilder::buildPelcoD(l.irAddress, 0x00, 0x20, 0x00, speed);
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
        QByteArray pkt = ProtocolBuilder::buildViscaFocus(l.visAddress, true);
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
    if (target == 0) {
        // 可见光：VISCA Focus Near
        QByteArray pkt = ProtocolBuilder::buildViscaFocus(l.visAddress, false);
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

// 点选跟踪
// 设置点击中心坐标，设备在 Distance 范围内搜索目标
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
// K1 开: FF 01 00 09 00 01 0B
// K1 关: FF 01 00 0B 00 01 0D
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

// ================= 电机串口管理 =================

bool DeviceController::openMotorSerial(const QString& portName)
{
    closeMotorSerial();
    m_motorSerial = new QSerialPort(portName, this);
    m_motorSerial->setBaudRate(QSerialPort::Baud9600);
    m_motorSerial->setDataBits(QSerialPort::Data8);
    m_motorSerial->setParity(QSerialPort::NoParity);
    m_motorSerial->setStopBits(QSerialPort::OneStop);
    if (!m_motorSerial->open(QIODevice::ReadWrite)) {
        emit motorSerialError(m_motorSerial->errorString());
        delete m_motorSerial;
        m_motorSerial = nullptr;
        return false;
    }
    connect(m_motorSerial, &QSerialPort::readyRead, this, [this]() {
        QByteArray data = m_motorSerial->readAll();
        if (data.size() >= 5 && static_cast<quint8>(data.at(1)) == 0x03) {
            bool isManual = (static_cast<quint8>(data.at(3)) == 0x01);
            emit motorModeResult(isManual);
        }
        emit commandSent("MODBUS_RECV", data);
    });
    connect(m_motorSerial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err != QSerialPort::NoError)
            emit motorSerialError(m_motorSerial->errorString());
    });
    return true;
}

void DeviceController::closeMotorSerial()
{
    if (m_motorSerial) {
        m_motorSerial->close();
        m_motorSerial->deleteLater();
        m_motorSerial = nullptr;
    }
}

bool DeviceController::isMotorSerialOpen() const
{
    return m_motorSerial && m_motorSerial->isOpen();
}

void DeviceController::sendModbus(const QByteArray& pkt)
{
    if (m_cfg->motorCommandChannel() == "串口") {
        if (!m_motorSerial || !m_motorSerial->isOpen()) {
            emit motorSerialError(tr("电机串口未打开"));
            return;
        }
        m_motorSerial->write(pkt);
        emit commandSent("MODBUS-RTU(SERIAL)", pkt);
    } else {
        // 默认走 Pelco-D 透传
        sendPelcoDWiper(pkt);
    }
}

void DeviceController::sendPelcoDWiper(const QByteArray& pkt)
{
    sendTransparentData("PELCO_D", pkt);
}


static uint16_t calculateModbusCRC16(const QByteArray &data) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < data.size(); pos++) {
        crc ^= (uint8_t)data[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ================= 电机 TCP 管理 (STM32-TCP-V4.0) =================


void DeviceController::openMotorTcp()
{
    closeMotorTcp();
    m_motorTcpSocket = new QTcpSocket(this);
    connect(m_motorTcpSocket, &QTcpSocket::readyRead, this, [this]() {
        QByteArray data = m_motorTcpSocket->readAll();
        // Here we could parse the JSON response from the motor, e.g. for motorCheckMode
        emit commandSent("STM32-TCP_RECV", data);
    });
    connect(m_motorTcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError err) {
        Q_UNUSED(err);
        emit motorSerialError(m_motorTcpSocket->errorString());
    });
    m_motorTcpSocket->connectToHost(m_cfg->motorTcpIp(), m_cfg->motorTcpPort());
}

void DeviceController::closeMotorTcp()
{
    if (m_motorTcpSocket) {
        m_motorTcpSocket->abort();
        m_motorTcpSocket->deleteLater();
        m_motorTcpSocket = nullptr;
    }
}

bool DeviceController::isMotorTcpOpen() const
{
    return m_motorTcpSocket && m_motorTcpSocket->state() == QAbstractSocket::ConnectedState;
}

void DeviceController::sendMotorTcpV4(const QJsonObject& json)
{
    if (!m_motorTcpSocket || m_motorTcpSocket->state() != QAbstractSocket::ConnectedState) {
        // Try to connect if not connected
        if (m_motorTcpSocket) {
            m_motorTcpSocket->connectToHost(m_cfg->motorTcpIp(), m_cfg->motorTcpPort());
            m_motorTcpSocket->waitForConnected(500);
        }
        if (!m_motorTcpSocket || m_motorTcpSocket->state() != QAbstractSocket::ConnectedState) {
            emit motorSerialError(tr("电机 TCP 未连接"));
            return;
        }
    }
    
    QJsonDocument doc(json);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    
    QByteArray header;
    header.resize(8);
    // Magic: 0xA55A (小端序 -> 5A A5)
    header[0] = static_cast<char>(0x5A);
    header[1] = static_cast<char>(0xA5);
    header[2] = static_cast<char>(0x02); // Cmd: 0x02
    quint16 len = payload.size();
    header[3] = static_cast<char>(len & 0xFF);
    header[4] = static_cast<char>((len >> 8) & 0xFF);
    header[5] = static_cast<char>(++m_motorTcpSeq & 0xFF);
    header[6] = 0x00; // CRC
    header[7] = 0x00; // CRC

    QByteArray pkt = header + payload;
    m_motorTcpSocket->write(pkt);
    emit commandSent("STM32-TCP-V4.0", pkt);
}

// ================= 雨刷电机控制 =================

void DeviceController::motorStart()
{
    if (m_cfg->motorProtocol() == "MODBUS-RTU") {
        sendModbus(QByteArray::fromHex("01060037001039C8"));
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["cmd_id"] = m_motorTcpSeq;
        cmd["action"] = 5;
        sendMotorTcpV4(cmd);
    } else {
        sendPelcoDWiper(QByteArray::fromHex("FF01000900010B"));
    }
}

void DeviceController::motorStop()
{
    if (m_cfg->motorProtocol() == "MODBUS-RTU") {
        sendModbus(QByteArray::fromHex("0106003800000807"));
    } else if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["cmd_id"] = m_motorTcpSeq;
        cmd["action"] = 2;
        sendMotorTcpV4(cmd);
    } else {
        sendPelcoDWiper(QByteArray::fromHex("FF01000B00010D"));
    }
}

void DeviceController::motorJogLeft()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["cmd_id"] = m_motorTcpSeq;
        cmd["action"] = 1;
        cmd["target_pos"] = 0;
        cmd["speed"] = 30000;
        sendMotorTcpV4(cmd);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037008039A4"));
}

void DeviceController::motorJogRight()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        QJsonObject cmd;
        cmd["cmd_id"] = m_motorTcpSeq;
        cmd["action"] = 1;
        cmd["target_pos"] = 100000;
        cmd["speed"] = 30000;
        sendMotorTcpV4(cmd);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037004039F4"));
}

void DeviceController::motorZeroCalib()
{
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("0106003A00016807"));
}

void DeviceController::motorReturnZero()
{
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("01060037000439C7"));
}

void DeviceController::motorCheckMode()
{
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    sendModbus(QByteArray::fromHex("010301B10001D5D1"));
}

void DeviceController::motorToggleMode()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_motorTcpIsAuto = !m_motorTcpIsAuto;
        QJsonObject cmd;
        cmd["cmd_id"] = m_motorTcpSeq;
        cmd["action"] = m_motorTcpIsAuto ? 11 : 10;
        sendMotorTcpV4(cmd);
        // 通知界面更新模式结果
        emit motorModeResult(!m_motorTcpIsAuto);
        return;
    }
    if (m_cfg->motorProtocol() != "MODBUS-RTU") return;
    
    m_motorModbusIsAuto = !m_motorModbusIsAuto;
    if (m_motorModbusIsAuto) {
        // 切换到自动模式
        sendModbus(QByteArray::fromHex("011001B100030600140009000AD4A8"));
    } else {
        // 切换到手动模式
        sendModbus(QByteArray::fromHex("011001B1000306000000000000B4AE"));
        QTimer::singleShot(50, this, [this]() {
            sendModbus(QByteArray::fromHex("010600380001C9C7"));
        });
        QTimer::singleShot(100, this, [this]() {
            sendModbus(QByteArray::fromHex("01060037000439C7")); // 触发绝对位置模式启动(回零)
        });
    }
    emit motorModeResult(!m_motorModbusIsAuto); // isManual = !isAuto
}

void DeviceController::motorToggleSilentMode()
{
    if (m_cfg->motorProtocol() == "STM32-TCP-V4.0") {
        m_motorTcpIsSilent = !m_motorTcpIsSilent;
        QJsonObject cmd;
        cmd["cmd_id"] = m_motorTcpSeq;
        cmd["action"] = m_motorTcpIsSilent ? 6 : 7;
        sendMotorTcpV4(cmd);
        // 通知界面更新按钮文字
        emit motorSilentResult(m_motorTcpIsSilent);
    }
}

void DeviceController::motorSetCurrent(int ma)
{
    if (ma < 0) ma = 0;
    if (ma > 2000) ma = 2000;
    
    // 构造设置电流指令
    QByteArray pkt;
    pkt.append((char)0x01);
    pkt.append((char)0x06);
    pkt.append((char)0x00);
    pkt.append((char)0x1E);
    pkt.append((char)((ma >> 8) & 0xFF));
    pkt.append((char)(ma & 0xFF));
    
    uint16_t crc = calculateModbusCRC16(pkt);
    pkt.append((char)(crc & 0xFF));
    pkt.append((char)((crc >> 8) & 0xFF));
    
    // 根据通道配置下发
    sendModbus(pkt);
    
    // 延时50ms后发送固化指令
    QTimer::singleShot(50, this, [this]() {
        QByteArray savePkt = QByteArray::fromHex("010600178000580E");
        sendModbus(savePkt);
    });
}
